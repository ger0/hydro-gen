#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

// (dirt, rock, water, total)
layout (binding = BIND_HEIGHTMAP, rgba32f)
    uniform writeonly image2D dest_tex;

uniform float height_scale;
uniform float water_lvl;

// Function to generate a random float in the range [0, 1]
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

#define SEED 265.0

void main() {
    ivec2 store_pos = ivec2(gl_GlobalInvocationID.xy);

    gln_tFBMOpts opts_ridge = gln_tFBMOpts(SEED, 0.50, 2.0, 0.002, 1, 1, true, true);
    float val_ridge = cos(((gln_sfbm(store_pos, opts_ridge) + 1) / 1.5) / 500.f);
    val_ridge = 0.5 * val_ridge + 0.5;

    gln_tFBMOpts opts = gln_tFBMOpts(SEED, 0.44, 2.0, 0.0025, 1, 8, false, false);
    float val = (gln_sfbm(store_pos, opts) + 1) / 2;
    // val *= val_ridge;
    val *= (sin(gl_GlobalInvocationID.x / 800.f) * 0.6 + 0.4);
    //float slope = (gl_GlobalInvocationID.x / (gl_NumWorkGroups.x * WRKGRP_SIZE_X));
    val *= val_ridge;
    val *= ((pow(val + 0.5, 3) - 0.125) / 3.25) * 0.55 + 0.45;
    val *= 1.65;
    // val = val * (exp(val) - 1) / 1.718;
    //val *= val * val;
    // val *= slope;
    // give the terrain a little slope
    // water height
    //terrain.b = max(0.0, water_lvl - terrain.r);
    //terrain.b += 1.4;
    vec4 terrain = vec4(val * height_scale, 0.0, 0.0, 0.0);
    terrain.w = terrain.r + terrain.b;

    imageStore(dest_tex, store_pos, terrain);
}
