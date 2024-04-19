#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

// (dirt, rock, water, total)
layout (binding = BIND_HEIGHTMAP, rgba32f)
    uniform writeonly image2D dest_tex;

layout (binding = BIND_FLUXMAP, rgba32f)
    uniform writeonly image2D flux_tex;

layout (binding = BIND_VELOCITYMAP, rgba32f)
    uniform writeonly image2D vel_tex;

uniform float height_scale;
uniform float water_lvl;

// Function to generate a random float in the range [0, 1]
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

#define SEED 0.0

void main() {
    ivec2 store_pos = ivec2(gl_GlobalInvocationID.xy);

    gln_tFBMOpts opts = gln_tFBMOpts(SEED, 0.4, 2.0, 0.004, 1, 7, false, false);
    float val = (gln_sfbm(store_pos, opts) + 1) / 2;
    //val = (pow(val + 0.5, 3) - 0.125) / 3.25;
    vec4 terrain = vec4(val * height_scale, 0.0, 0.0, 0.0);
    // water height
    //terrain.b = max(0.0, water_lvl - terrain.r);
    terrain.b += 1.4;
    terrain.w = terrain.r + terrain.b;

    imageStore(dest_tex, store_pos, terrain);
    imageStore(vel_tex, store_pos, vec4(0,0,0,0));
    imageStore(flux_tex, store_pos, vec4(0,0,0,0));
}
