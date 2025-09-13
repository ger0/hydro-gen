#ifndef HYDR_SETS_HPP
#define HYDR_SETS_HPP

#include <glm/glm.hpp>
#include "shaderprogram.hpp"
#include "glsl/bindings.glsl"

namespace State {

constexpr float MAX_HEIGHT = 256.f;
constexpr float WATER_HEIGHT = 96.f;

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

        .hmap_dims      = glm::ivec2(1024, 1024),
        .height_mult    = 1.0,
        .water_lvl      = WATER_HEIGHT,
        .seed           = 10000.f * rand() / (float)RAND_MAX,
        .persistance    = 0.44,
        .lacunarity     = 2.0,
        .scale          = 0.00075,
        .redistribution = 1, // doesn't work
        .octaves        = 8,
        .mask_round     = false,
        .mask_exp       = true,
        .mask_power     = true,
        .mask_slope     = false,

        .uplift         = 0,
        .uplift_scale   = 1.16,

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
    Erosion_data data;
    void push_data() {
        buffer.push_data(data);
    }
};

};

#endif //HYDR_SETS_HPP
