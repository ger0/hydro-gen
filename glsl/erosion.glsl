#version 460

#include "bindings.glsl"

layout (local_size_x = 8, local_size_y = 8) in;

// (dirt height, rock height, water height, total height)
layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

// (fL, fR, fT, fB) left, right, top, bottom
layout (binding = BIND_FLUXMAP, rgba32f)   
	uniform readonly image2D fluxmap;
layout (binding = BIND_WRITE_FLUXMAP, rgba32f)   
	uniform writeonly image2D out_fluxmap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;

uniform float max_height;
float max_flux = max_height * 1000000000;

#define PI 3.1415926538

// cross-section area of a pipe
const float A = 1.0;
// length of the pipe
const float L = 1.0;
// gravity acceleration
const float G = 9.81;
// time step
const float d_t = 0.008;
// sediment capacity constant
const float Kc = 0.03;
// sediment dissolving constant
const float Ks = 0.03;
// sediment deposition constant
const float Kd = 0.03;

vec4 get_flux(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y)) {
       return vec4(0, 0, 0, 0); 
    }
    return imageLoad(fluxmap, pos);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    
    vec4 out_flux = get_flux(pos);
    vec4 vel      = imageLoad(velocitymap, pos);

    // water height
    vec4 terrain = imageLoad(heightmap, pos);
    float d1 = terrain.b;

    // total height difference
    vec4 d_height;
    d_height.x = terrain.w - imageLoad(heightmap, pos + ivec2(-1, 0)).w; // left
    d_height.y = terrain.w - imageLoad(heightmap, pos + ivec2( 1, 0)).w; // right
    d_height.z = terrain.w - imageLoad(heightmap, pos + ivec2( 0, 1)).w; // top
    d_height.w = terrain.w - imageLoad(heightmap, pos + ivec2( 0,-1)).w; // bottom

    vec4 in_flux;
    in_flux.x = get_flux(pos + ivec2(-1, 0)).y; // from left
    in_flux.y = get_flux(pos + ivec2( 1, 0)).x; // from right
    in_flux.z = get_flux(pos + ivec2( 0, 1)).w; // from top
    in_flux.w = get_flux(pos + ivec2( 0,-1)).z; // from bottom 

    // minimum - 0, max: height * 1/time * d_height
    out_flux.x = min(
        max(0.0, out_flux.x + d_t * A * (G * d_height.x) / L),
        max(0, 1 / d_t * max_flux * d_height.x)
    );
    out_flux.y = min(
        max(0.0, out_flux.y + d_t * A * (G * d_height.y) / L),
        max(0, 1 / d_t * max_flux * d_height.y)
    );
    out_flux.z = min(
        max(0.0, out_flux.z + d_t * A * (G * d_height.z) / L),
        max(0, 1 / d_t * max_flux * d_height.z)
    );
    out_flux.w = min(
        max(0.0, out_flux.w + d_t * A * (G * d_height.w) / L),
        max(0, 1 / d_t * max_flux * d_height.w)
    );

    // boundary checking
    if (pos.x <= 0) {
        out_flux.x = 0.0;
    } else if (pos.x >= (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1)) {
        out_flux.y = 0.0;
    } 
    if (pos.y <= 0) {
        out_flux.w = 0.0;
    } else if (pos.y >= (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        out_flux.z = 0.0;
    } 

    // scaling factor
    float sum_out_flux = out_flux.x + out_flux.y + out_flux.z + out_flux.w;
    float K = min(1, (d1 * L * L) / (sum_out_flux * d_t));
    out_flux *= K;
    sum_out_flux *= K;

    float sum_in_flux = in_flux.x + in_flux.y + in_flux.z + in_flux.w;
    float d_volume = d_t * (sum_in_flux - sum_out_flux);
    float d2 = d1 + (d_volume / (L * L));

    // water velocity
    float d_Wx = (
        imageLoad(fluxmap, pos + ivec2(-1, 0)).y -
        imageLoad(fluxmap, pos).x + 
        imageLoad(fluxmap, pos).y -
        imageLoad(fluxmap, pos + ivec2(1, 0)).x
    ) / 2.0;
    float u = d_Wx / (L * (d1 + d2) / 2);

    float d_Wy = (
        imageLoad(fluxmap, pos + ivec2(0, -1)).z -
        imageLoad(fluxmap, pos).w + 
        imageLoad(fluxmap, pos).z -
        imageLoad(fluxmap, pos + ivec2(0, 1)).w
    ) / 2.0;
    float v = d_Wy / (L * (d1 + d2) / 2);
    //float cos_tilt = dot(normalize(vec3(dx, 1.0, dz)), vec3(0, 1.0, 0));
    //float tilt_angle = 0* acos(cos_tilt);

    //
	float r_b = imageLoad(heightmap, pos + ivec2(1, 0)).x;
	float l_b = imageLoad(heightmap, pos - ivec2(1, 0)).x;
	float d_b = imageLoad(heightmap, pos + ivec2(0, 1)).x;
	float u_b = imageLoad(heightmap, pos - ivec2(0, 1)).x;

	float dbdx = (r_b-l_b) / (2.0);
	float dbdy = (r_b-l_b) / (2.0);

	float sin_alpha = sqrt(dbdx*dbdx+dbdy*dbdy)/sqrt(1+dbdx*dbdx+dbdy*dbdy);

    // sediment transport capacity
    //float c = Kc * sqrt(1 - (cos_tilt * cos_tilt)) * length(vec2(u, v));
    //float c = Kc * sin(tilt_angle) * length(vec2(u, v));
    float c = Kc * sin_alpha * length(vec2(u, v));

    float st = imageLoad(velocitymap, pos).z;
    float bt;
    float s1;
    // dissolve sediment
    if (c > st) {
        bt = terrain.r - Ks * (c - st);
        s1 = st + Ks * (c - st);
    } 
    // deposit sediment
    else {
        bt = terrain.r + Kd * (st - c);
        s1 = st - Kd * (st - c);
    }

    //vec4 end_terrain = vec4(bt, 0.0, d2, bt + d2);
    vec4 end_terrain = vec4(terrain.r, 0.0, d2, terrain.r + d2);
    imageStore(out_heightmap, pos, end_terrain);
    imageStore(out_fluxmap, pos, out_flux);
    imageStore(out_velocitymap, pos, vec4(u, v, s1, 0));
}
