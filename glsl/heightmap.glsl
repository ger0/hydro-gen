#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
#line 6

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

// (dirt, rock, water, total)
layout (binding = BIND_HEIGHTMAP, rgba32f)
    uniform writeonly image2D dest_tex;

layout (binding = BIND_VELOCITYMAP, rgba32f)
    uniform writeonly image2D dest_vel;

layout (binding = BIND_FLUXMAP, rgba32f)
    uniform writeonly image2D dest_flux;

layout (binding = BIND_SEDIMENTMAP, rgba32f)
    uniform writeonly image2D dest_sediment;

layout (std140, binding = BIND_UNIFORM_MAP_SETTINGS)
uniform map_settings {
    Map_settings_data cfg;
};

// Function to generate a random float in the range [0, 1]
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float round_mask(float val, vec2 uv) {
    float v = 1.0 - (pow(uv.x - 0.5, 2.0) + pow(uv.y - 0.5, 2.0) + 0.75);
    v = pow(v, 1.0 / 2.0);
    return val * max(0.0, v * 1.0);
}

float slope_mask(float v, vec2 uv) {
    return 0.25 * v + 0.75 * (v * uv.x * uv.y);
}

float power_mask(float val) {
    return val * (((pow(val + 0.5, 3) - 0.125) / 3.25) * 0.55 + 0.45);
}

float exp_mask(float val) {
    return val * (exp(val) - 1) / 1.718;
}

float distort(float x, float y) {
    float wiggleDensity = 4.7f; 
    return gln_simplex(vec2(x * wiggleDensity, y * wiggleDensity));
}

void main() {
    ivec2 store_pos = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = gl_GlobalInvocationID.xy / vec2(imageSize(dest_tex).xy);
    gln_tFBMOpts opts = gln_tFBMOpts(
        cfg.seed,
        cfg.persistance,
        cfg.lacunarity,
        cfg.scale,
        1.0,
        cfg.octaves,
        false,
        false
    );
    /* gln_tFBMOpts up_opts = gln_tFBMOpts(
        cfg.seed,
        cfg.persistance,
        cfg.lacunarity,
        cfg.scale,
        2.0,
        cfg.octaves,
        true,
        true
    ); */

    vec2 dist = vec2(
        distort(store_pos.x / 1e7 + 2.3, store_pos.y / 1e7 + 2.9),
        distort(store_pos.x / 1e7 - 3.1, store_pos.y / 1e7 - 4.3)
    );

    float val = (gln_sfbm(vec2(store_pos) + dist, opts) + 1) / 2.0;
    // float val = (gln_sfbm(vec2(store_pos), opts) + 1) / 2.0;
    //float up_val = (gln_sfbm(store_pos, up_opts) + 1.0) / 2.0;
    //up_val = power_mask(up_val);
    //up_val = exp_mask(up_val);
    /* val += up_val * cfg.height_mult;
    val /= 2.0; */
    
    if (cfg.mask_round != 0) {
        val = round_mask(val, uv);
    }
    if (cfg.mask_exp != 0) {
        val = exp_mask(val);
    }
    if (cfg.mask_power != 0) {
        val = power_mask(val);
    }
    if (cfg.mask_slope != 0) {
        val = slope_mask(val, uv);
    }
    
    float rock_val = min(cfg.max_height, val * cfg.max_height * cfg.height_mult);
    float dirt_val = 10.0;

    vec4 terrain = vec4(
        rock_val,
        dirt_val,
        0.0, 
        0.0
    );
    /* terrain = vec4(
        store_pos.x * 2.0,
        0,
        0,
        0
    ); */
    /* terrain = vec4(
        10.0,
        20.0, 
        0.0, 
        0.0
    ); */
    // water height
    // terrain.b += 84.4;
    // terrain.b = max(0.0, 84.0 - terrain.r);
    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(dest_tex, store_pos, terrain);
    imageStore(dest_vel, store_pos, vec4(0));
    imageStore(dest_flux, store_pos, vec4(0));
    imageStore(dest_sediment, store_pos, vec4(0));
    //imageStore(dest_tex, store_pos, vec4(cfg.seed, cfg.lacunarity, cfg.persistance, cfg.cfg.max_height));
}
