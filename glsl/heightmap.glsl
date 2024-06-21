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

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};

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
    vec2 dist = vec2(1, 1);
    if (cfg.domain_warp != 0) {
        dist = vec2(
            gln_sfbm(vec2(store_pos.x + 2.3, store_pos.y + 2.9), opts),
            gln_sfbm(vec2(store_pos.x - 3.1, store_pos.y - 4.3), opts)
        );
        if (cfg.domain_warp == 2) {
            dist = vec2(
                gln_sfbm(vec2(store_pos.x + cfg.domain_warp_scale* dist.x - 5.7, store_pos.y + cfg.domain_warp_scale*dist.y + 27.9), opts),
                gln_sfbm(vec2(store_pos.x + cfg.domain_warp_scale* dist.x + 11.5, store_pos.y + cfg.domain_warp_scale*dist.y - 23.7), opts)
            );
        }
    }
    float val = 0.0;
    if (cfg.fake_erosion != 0) {
        val = perlfbm(vec2(store_pos) + cfg.domain_warp_scale * dist, opts);
    } else {
        val = (gln_sfbm(vec2(store_pos) + cfg.domain_warp_scale * dist, opts) + 1.0) / 2.0;
    }

    if (cfg.uplift != 0) {
        gln_tFBMOpts up_opts = gln_tFBMOpts(
            cfg.seed,
            cfg.persistance,
            cfg.lacunarity,
            cfg.scale / cfg.uplift_scale,
            1.0,
            cfg.octaves,
            true,
            true
        );
        float up = gln_sfbm(vec2(store_pos.x - 7.3, store_pos.y + 19.9), up_opts);
        val *= up;
    }
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

    if (cfg.terrace > 0) {
        int levels = cfg.terrace;
        float lol = floor(val / (1.0 / levels));
        float t_scl = cfg.terrace_scale;
        val = (t_scl * lol * (1.0 / levels)) + val * (1 - t_scl);
    }

    float rock_val = min(cfg.max_height, val * cfg.max_height * cfg.height_mult);
    float dirt_val = 2.0;
    dirt_val = gln_sfbm(store_pos + vec2(13.7, 27.1), opts) + 1.0;
    dirt_val *= cfg.max_dirt;

    vec4 terrain = vec4(
        rock_val,
        dirt_val,
        0.0, 
        0.0
    );
    // water height
    // terrain.b += 84.4;
    // terrain.b = max(0.0, 84.0 - terrain.r);
    terrain.w = terrain.r + terrain.g + terrain.b;
#if defined(PARTICLE_COUNT)
    if (gl_GlobalInvocationID.xy == vec2(0, 0)) {
        for (uint i = 0; i < PARTICLE_COUNT; i++) {
            particles[i].iters = 0;
        }
    }
#endif
    imageStore(dest_tex, store_pos, terrain);
    imageStore(dest_vel, store_pos, vec4(0));
    imageStore(dest_flux, store_pos, vec4(0));
    imageStore(dest_sediment, store_pos, vec4(0));
}
