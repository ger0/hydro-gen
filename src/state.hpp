#ifndef HYDR_STATE_HPP
#define HYDR_STATE_HPP

#include "shaderprogram.hpp"
#include <glm/glm.hpp>
#include "settings.hpp"

namespace State {

struct Settings {
    Erosion_settings erosion;
    Rain_settings rain;
    Map_settings map;
};

Settings setup_settings(bool is_particle = false, u32 particle_count = 0);
void delete_settings(Settings& settings);

struct Program_state {
    bool should_rain = true;
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

    float target_fps = 66.f;
    struct Camera {
        glm::vec3 pos    = glm::vec3(0.f, State::MAX_HEIGHT, 0.f);
        glm::vec3 dir    = glm::vec3(0.f, State::MAX_HEIGHT, -1.f);

        glm::vec3 _def_dir = normalize(pos - dir);
	    static constexpr float speed = 0.05f;

        glm::vec3 right  = glm::normalize(glm::cross(glm::vec3(0,1,0), _def_dir));
	    glm::vec3 up     = glm::cross(_def_dir, right);

	    float yaw   = -90.f;
	    float pitch = 0.f;

	    bool boost  = false;	
    } camera;
};

// all OpenGL textures representing world state
namespace World {
struct Textures {
    GLfloat time;
    u32 map_size;
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

Textures gen_textures(const GLuint size);
void delete_textures(Textures& data);

void gen_heightmap(
    Settings& settings,
    State::World::Textures& world_data,
    Compute_program& program
);

};
};
#endif //HYDR_STATE_HPP
