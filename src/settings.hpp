#ifndef HYDR_SETS_HPP
#define HYDR_SETS_HPP

#include <glm/glm.hpp>
#include "shaderprogram.hpp"
#include "glsl/bindings.glsl"

namespace State {

constexpr float MAX_HEIGHT = 256.f;
constexpr float WATER_HEIGHT = 96.f;
constexpr GLuint NOISE_SIZE = 1024;

struct Rain_settings {
    gl::Buffer buffer;
    Rain_data data {
        .amount = 0.01f,
        .mountain_thresh = 0.55f,
        .mountain_multip = 0.05f,
        .period = 512,
        .drops = 0.02f
    };
    void push_data() {
        buffer.push_data(data);
    }
};

struct Map_settings {
    gl::Buffer buffer;
    struct Map_settings_data data {
        .max_height     = MAX_HEIGHT,
            
        .max_dirt       = 2.0,

        .hmap_dims      = glm::ivec2(NOISE_SIZE, NOISE_SIZE),
        .height_mult    = 1.0,
        .water_lvl      = WATER_HEIGHT,
        .seed           = 10000.f * rand() / (float)RAND_MAX,
        .persistance    = 0.44,
        .lacunarity     = 2.0,
        .scale          = 0.0015,
        .redistribution = 1, // doesn't work
        .octaves        = 8,
        .mask_round     = false,
        .mask_exp       = false,
        .mask_power     = true,
        .mask_slope     = false,

        .uplift         = 0,
        .uplift_scale   = 1,

        .domain_warp    = 1,
        .domain_warp_scale = 100.f,
        .terrace        = 0,
        .terrace_scale  = 0.5
    };
    void push_data() {
        buffer.push_data(data);
    }
};

struct Erosion_settings {
    gl::Buffer buffer;
    Erosion_data data = {
        .Kc             = 0.2000,
        .Kalpha         = VEC2(1.3f, 0.6f),
        .Kconv          = 0.001,
        .Ks             = VEC2(0.03, 0.09),
        .Kd             = VEC2(0.01, 0.03),
        .Ke             = 0.03,
#if defined(PARTICLE_COUNT)
        .Kspeed         = VEC2(0.002f, 0.008f),
        .G              = 9.81,
        .d_t            = 0.25,
        .density        = 1.0,
        .init_volume    = 1.0,
        .friction       = 0.20,
        .inertia        = 1.00,
        .min_volume     = 0.00,
        .min_velocity   = 0.001,
        .ttl            = 15000,
#else 
        .ENERGY_KEPT    = 1.0,
        .Kspeed         = VEC2(0.5f, 2.0f),
        .G              = 1.0,
        .d_t            = 0.001,
#endif
    };
    void push_data() {
        buffer.push_data(data);
    }
};

};

#endif //HYDR_SETS_HPP
