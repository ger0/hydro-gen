#ifndef HYDR_STATE_HPP
#define HYDR_STATE_HPP

#include "src/shaderprogram.hpp"
#include <glm/glm.hpp>
#include "glsl/bindings.glsl"

constexpr float MAX_HEIGHT = 256.f;
constexpr float WATER_HEIGHT = 96.f;
constexpr GLuint NOISE_SIZE = 256;

namespace State {

static struct Global_state {
    bool should_rain = false;
    bool should_erode = true;
    bool should_render = true;

    bool mouse_disabled = false;

    bool shader_error = false;

    float delta_frame = 0.f;
    float last_frame = 0.f;

    u32 frame_count = 0;
    float last_frame_rounded = 0.0;
    double frame_t = 0.0;

    u32 erosion_steps = 0;
    float erosion_time;
    float erosion_mean_t = 0.f;

    float target_fps = 60.f;
} global;

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

static struct Settings {
    Erosion_settings erosion;
    Rain_settings rain;
    Map_settings map;
} settings;

void setup_settings();
void delete_settings();

// all OpenGL textures representing world state
namespace World {
struct Textures {
    GLfloat time;
    gl::Tex_pair heightmap;

    // hydraulic erosion
    gl::Tex_pair flux;
    gl::Tex_pair velocity;
    gl::Tex_pair sediment;

    // thermal erosion
    gl::Tex_pair thermal_c;
    gl::Tex_pair thermal_d;

    gl::Texture lockmap;
    gl::Buffer particle_buffer;
};

Textures gen_textures(const GLuint width, const GLuint height);
void delete_textures(Textures& data);

void gen_heightmap(Compute_program& program);

};

};
#endif //HYDR_STATE_HPP
