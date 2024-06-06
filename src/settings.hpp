#ifndef HYDR_SETS_HPP
#define HYDR_SETS_HPP

#include <glm/glm.hpp>
#include "shaderprogram.hpp"
#include "glsl/bindings.glsl"

namespace State {

constexpr float MAX_HEIGHT = 256.f;
constexpr float WATER_HEIGHT = 96.f;
constexpr GLuint NOISE_SIZE = 256;

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
        .hmap_dims      = glm::ivec2(NOISE_SIZE, NOISE_SIZE),
        .height_mult    = 1.0,
        .water_lvl      = WATER_HEIGHT,
        .seed           = 10000.f * rand() / (float)RAND_MAX,
        .persistance    = 0.44,
        .lacunarity     = 2.0,
        .scale          = 0.0025,
        .redistribution = 1, // doesn't work
        .octaves        = 8,
        .mask_round     = false,
        .mask_exp       = false,
        .mask_power     = false,
        .mask_slope     = false
    };
    void push_data() {
        buffer.push_data(data);
    }
};

struct Erosion_settings {
    gl::Buffer buffer;
    Erosion_data data = {
        .Kc             = 0.60,
        .Ks             = 0.036,
        .Kd             = 0.02,
        .Ke             = 0.003,
        .Kalpha         = 1.2f,
#if defined(PARTICLE_COUNT)
        .Kspeed         = 0.001f,
        .G              = 9.81,
        .d_t            = 0.75,
        .density        = 1.0,
        .init_volume    = 1.0,
        .friction       = 0.2,
        .inertia        = 0.40,
        .min_volume     = 0.1,
        .min_velocity   = 0.10,
        .ttl            = 5000,
#else 
        .ENERGY_KEPT    = 1.0,
        .Kspeed         = 0.1f,
        .G              = 1.0,
        .d_t            = 0.003,
#endif
    };
    void push_data() {
        buffer.push_data(data);
    }
};

};

#endif //HYDR_SETS_HPP
