#ifndef HYDR_RENDER_HPP
#define HYDR_RENDER_HPP

#include "shaderprogram.hpp"
#include "state.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Render {

constexpr float Z_NEAR = 0.1f;
constexpr float Z_FAR = 2048.f;
constexpr float FOV = 90.f;

// why does it have to have methods...
struct Data {
    GLuint framebuffer;
    gl::Texture output_texture;

    float   prec = 0.35;
    bool    display_sediment = false;
    bool    display_particles = false;
    bool    debug_preview = false;

    struct Dims {
        GLuint w;
        GLuint h;
    } window_dims;
    float   aspect_ratio;

    Compute_program shader;
    Data(
        GLuint window_width,
        GLuint window_height,
        GLuint noise_size,
        State::Settings& settings,
        State::Program_state& program_state,
        State::World::Textures& textures_data
    );
    ~Data();

    void blit();
    bool dispatch(
        State::World::Textures& world_data,
        State::Settings& settings,
        State::Program_state::Camera& camera
    );
    void handle_ui(
        State::Settings& settings,
        State::Program_state& state,
        State::World::Textures& world,
        Compute_program& map_generator
    );
};

};
#endif //HYDR_RENDER_HPP

