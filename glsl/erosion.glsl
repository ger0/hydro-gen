#version 460

#include "img_interpolation.glsl"
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
// vec3((u, v), )
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;

uniform float max_height;
float max_flux = max_height * 1000000000;

// cross-section area of a pipe
const float A = 1.0;
// length of the pipe
const float L = 1.0;
// gravity acceleration
const float G = 9.81;
// time step
uniform float d_t;

vec4 get_flux(ivec2 pos) {
    if (pos.x <= 0 || pos.x >= (gl_WorkGroupSize.x * gl_NumWorkGroups.x) ||
    pos.y <= 0 || pos.y >= (gl_WorkGroupSize.y * gl_NumWorkGroups.y)) {
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
    /* out_flux.x = 
        max(0.0, out_flux.x + d_t * A * (G * d_height.x) / L);
    out_flux.y = 
        max(0.0, out_flux.y + d_t * A * (G * d_height.y) / L);
    out_flux.z = 
        max(0.0, out_flux.z + d_t * A * (G * d_height.z) / L);
    out_flux.w =
        max(0.0, out_flux.w + d_t * A * (G * d_height.w) / L); */

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

    terrain.b = d2;
    terrain.w = terrain.r + d2;

    vel.z = (d1 + d2) / 2.0; 
    imageStore(out_fluxmap, pos, out_flux);
    imageStore(out_velocitymap, pos, vel);
    imageStore(out_heightmap, pos, terrain);
}
