#version 460

#include "img_interpolation.glsl"
#include "bindings.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

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

// cross-section area of a pipe
const float A = 1.0;
// length of the pipe
const float L = 1.0;
// gravity acceleration
const float G = 9.81;
// time step
uniform float d_t;

float get_rheight(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return 999999999999.0;
    }
    return imageLoad(heightmap, pos).r;
}

float get_wheight(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return 999999999999.0;
    }
    return imageLoad(heightmap, pos).w;
}

vec4 get_flux(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
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
    d_height.x = terrain.w - get_wheight(pos + ivec2(-1, 0)); // left
    d_height.y = terrain.w - get_wheight(pos + ivec2( 1, 0)); // right
    d_height.z = terrain.w - get_wheight(pos + ivec2( 0, 1)); // top
    d_height.w = terrain.w - get_wheight(pos + ivec2( 0,-1)); // bottom

    // terrain slope
    vec2 slope;
    slope.x = (get_wheight(pos + ivec2( 1, 0)) - get_wheight(pos + ivec2(-1, 0))) / 2.0; // dx
    slope.y = (get_wheight(pos + ivec2( 0, 1)) - get_wheight(pos + ivec2( 0,-1))) / 2.0; // dz

    vec4 in_flux;
    in_flux.x = get_flux(pos + ivec2(-1, 0)).y; // from left
    in_flux.y = get_flux(pos + ivec2( 1, 0)).x; // from right
    in_flux.z = get_flux(pos + ivec2( 0, 1)).w; // from top
    in_flux.w = get_flux(pos + ivec2( 0,-1)).z; // from bottom 

    /* out_flux.x += slope.x;
    out_flux.y += -slope.x;
    out_flux.z += -slope.y;
    out_flux.w += slope.y; */

    #define LOSS 0.99985
    out_flux.x = 
        max(1e-32, LOSS * out_flux.x + d_t * A * (G * d_height.x) / L);
    out_flux.y =  
        max(1e-32, LOSS * out_flux.y + d_t * A * (G * d_height.y) / L);
    out_flux.z =  
        max(1e-32, LOSS * out_flux.z + d_t * A * (G * d_height.z) / L);
    out_flux.w =
        max(1e-32, LOSS * out_flux.w + d_t * A * (G * d_height.w) / L);

    /* out_flux.x = min(1e-32, out_flux.x + d_t * A * (G * d_height.x) / L);
    out_flux.y = min(1e-32, out_flux.y + d_t * A * (G * d_height.y) / L);
    out_flux.z = min(1e-32, out_flux.z + d_t * A * (G * d_height.z) / L);
    out_flux.w = min(1e-32, out_flux.w + d_t * A * (G * d_height.w) / L); */

    // boundary checking */
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

    float sum_in_flux = in_flux.x + in_flux.y + in_flux.z + in_flux.w;
    float sum_out_flux = out_flux.x + out_flux.y + out_flux.z + out_flux.w;

     //scaling factor
     /* float K = min(1.0, (terrain.b * L * L) / (sum_out_flux * d_t));
     out_flux *= K;
     sum_out_flux *= K; */
    if ((sum_out_flux * d_t) > (L * L * terrain.b)) {
        float K = (terrain.b * L * L) / (sum_out_flux * d_t);
        out_flux *= K;
        sum_out_flux *= K;
    }
    float d_volume = d_t * (sum_in_flux - sum_out_flux);
    float d2 = d1 + (d_volume / (L * L));

    terrain.b = d2;
    terrain.w = terrain.r + d2;

    // average water height
    vel.z = max(0.001, (d1 + d2) / 2.0); 
    imageStore(out_fluxmap, pos, out_flux);
    imageStore(out_velocitymap, pos, vel);
    imageStore(out_heightmap, pos, terrain);
}
