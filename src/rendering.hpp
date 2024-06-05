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
    bool    debug_preview = false;

    struct Dims {
        GLuint w;
        GLuint h;
    } window_dims;
    float   aspect_ratio;

    Compute_program shader;
    Data(
        GLuint window_w,
        GLuint window_h,
        GLuint noise_size,
        State::World::Textures& data
    );
    ~Data();

    void blit();
    bool dispatch(
        State::World::Textures& data
    );
    void handle_ui(
        State::World::Textures& world_data, 
        Compute_program& comput_map
    );
};

};
#endif //HYDR_RENDER_HPP

