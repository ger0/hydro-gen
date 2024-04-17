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

#define PI 3.1415926538

// cross-section area of a pipe
const float A = 1.0;
// length of the pipe
const float L = 1.0;
// gravity acceleration
const float G = 0.981;
// time step
const float d_t = 0.04;
// sediment capacity constant
const float Kc = 0.3;
// sediment dissolving constant
const float Ks = 0.3;
// sediment deposition constant
const float Kd = 0.3;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    
    vec4 out_flux = imageLoad(fluxmap, pos);
    vec4 vel      = imageLoad(velocitymap, pos);

    // water height
    vec4 terrain = imageLoad(heightmap, pos);
    float d1 = terrain.b;

    // total height difference
    vec4 d_height;
    d_height.x = -imageLoad(heightmap, pos + ivec2(-1, 0)).w + terrain.w; // left
    d_height.y = -imageLoad(heightmap, pos + ivec2( 1, 0)).w + terrain.w; // right
    d_height.z = -imageLoad(heightmap, pos + ivec2( 0, 1)).w + terrain.w; // top
    d_height.w = -imageLoad(heightmap, pos + ivec2( 0,-1)).w + terrain.w; // bottom

    vec4 in_flux;
    in_flux.x = max(0, imageLoad(fluxmap, pos + ivec2(-1, 0)).y); // left
    in_flux.y = max(0, imageLoad(fluxmap, pos + ivec2( 1, 0)).x); // right
    in_flux.z = max(0, imageLoad(fluxmap, pos + ivec2( 0, 1)).w); // top
    in_flux.w = max(0, imageLoad(fluxmap, pos + ivec2( 0,-1)).z); // bottom 

    out_flux.x = max(0, out_flux.x + d_t * A * (G * d_height.x) / L);
    out_flux.y = max(0, out_flux.y + d_t * A * (G * d_height.y) / L);
    out_flux.z = max(0, out_flux.z + d_t * A * (G * d_height.z) / L);
    out_flux.w = max(0, out_flux.w + d_t * A * (G * d_height.w) / L);

    // scaling factor
    float sum_out_flux = out_flux.x + out_flux.y + out_flux.z + out_flux.w;
    float K = min(1, (d1 * L * L) / (sum_out_flux * d_t));

    out_flux *= K;
    sum_out_flux *= K;

    float sum_in_flux = in_flux.x + in_flux.y + in_flux.z + in_flux.w;
    float K2 = min(1, (d1 * L * L) / (sum_in_flux * d_t));

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

    float dx = imageLoad(heightmap, pos + ivec2(1, 0)).r - imageLoad(heightmap, pos + ivec2(-1, 0)).r;
    float dz = imageLoad(heightmap, pos + ivec2(0, 1)).r - imageLoad(heightmap, pos + ivec2( 0,-1)).r;
    float cos_tilt = dot(normalize(vec3(dx, 0, dz)), vec3(0, 1, 0));
    float tilt_angle = (PI / 2) - acos(cos_tilt);

    // sediment transport capacity
    // float c = Kc * sqrt(1 - (cos_tilt * cos_tilt)) * length(vec2(v, u));
    float c = Kc * sin(tilt_angle) * length(vec2(v, u));

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

    vec4 end_terrain = vec4(bt, 0.0, d2, bt + d2);
    imageStore(out_heightmap, pos, end_terrain);
    imageStore(out_fluxmap, pos, out_flux);
    imageStore(out_velocitymap, pos, vec4(u, v, s1, 0));
}
