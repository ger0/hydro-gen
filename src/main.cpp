#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "utils.hpp"

#include "../glsl/bindings.glsl"

constexpr u32 WINDOW_W = 1600;
constexpr u32 WINDOW_H = 900;

constexpr u32 PARTICLE_COUNT = 128;

constexpr float MAX_HEIGHT = 256.f;
constexpr float WATER_HEIGHT = 96.f;

constexpr float Z_NEAR = 0.1f;
constexpr float Z_FAR = 2048.f;
constexpr float FOV = 90.f;
constexpr float ASPECT_RATIO = float(WINDOW_W) / WINDOW_H;

constexpr GLuint NOISE_SIZE = 128;

// shader filenames
constexpr auto noise_comput_file                = "heightmap.glsl";
constexpr auto rain_comput_file                 = "rain.glsl";
constexpr auto render_comput_file               = "rendering.glsl";

constexpr auto erosion_hydro_flux_file          = "hydro_flux.glsl";
constexpr auto erosion_hydro_erosion_file       = "hydro_erosion.glsl";
constexpr auto erosion_thermal_flux_file        = "thermal_erosion.glsl";
constexpr auto erosion_thermal_transport_file   = "thermal_transport.glsl";
constexpr auto erosion_sediment_file            = "sediment_transport.glsl";
constexpr auto erosion_smooth_file              = "smoothing.glsl";
constexpr auto erosion_particle_file            = "particle.glsl";

using glm::normalize, glm::cross;
using glm::vec2, glm::vec3, glm::ivec2, glm::ivec3;
using glm::vec4, glm::ivec4;

// mouse position
static vec2 mouse_last;
static struct Camera {
	vec3 pos    = vec3(0.f, MAX_HEIGHT, 0.f);
	vec3 dir    = vec3(0.f, MAX_HEIGHT, -1.f);

	vec3 _def_dir = normalize(pos - dir);
	static constexpr float speed = 0.05f;

	vec3 right  = normalize(cross(vec3(0,1,0), _def_dir));
	vec3 up     = cross(_def_dir, right);

	float yaw   = -90.f;
	float pitch = 0.f;

	bool boost  = false;	
} camera;

static struct Game_state {
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
} state;


struct Rain_settings {
    gl::Buffer buffer;
    Rain_data data {
        .max_height = MAX_HEIGHT,
        .amount = 0.01f,
        .mountain_thresh = 0.55f,
        .mountain_multip = 0.05f,
        .period = 512,
        .drops = 0.02f
    };
    void push_data() {
        buffer.push_data(data);
    }
    Rain_settings() {
        buffer.binding = BIND_UNIFORM_RAIN_SETTINGS;
        gl::gen_buffer(buffer);
    }
    ~Rain_settings() {
        gl::del_buffer(buffer);
    }
};

struct Map_settings {
    gl::Buffer buffer;
    struct Data {
        alignas(sizeof(GLfloat)) GLfloat height_scale   = MAX_HEIGHT;
        alignas(sizeof(GLfloat)) GLfloat height_mult    = 1.0;
        alignas(sizeof(GLfloat)) GLfloat water_lvl      = WATER_HEIGHT;
        alignas(sizeof(GLfloat)) GLfloat seed           = 10000.f * rand() / (float)RAND_MAX;
        alignas(sizeof(GLfloat)) GLfloat persistance    = 0.44;
        alignas(sizeof(GLfloat)) GLfloat lacunarity     = 2.0;
        alignas(sizeof(GLfloat)) GLfloat scale          = 0.0025;
        alignas(sizeof(GLfloat)) GLfloat redistribution = 1; // doesn't work
        alignas(sizeof(GLint))   GLint   octaves        = 8;

        alignas(sizeof(GLuint))  GLuint  mask_round   = false;
        alignas(sizeof(GLuint))  GLuint  mask_exp     = false;
        alignas(sizeof(GLuint))  GLuint  mask_power   = false;
        alignas(sizeof(GLuint))  GLuint  mask_slope   = false;
    } data;
    void push_data() {
        buffer.push_data(data);
    }
    Map_settings() {
        buffer.binding = BIND_UNIFORM_MAP_SETTINGS;
        gl::gen_buffer(buffer);
    }
    ~Map_settings() {
        gl::del_buffer(buffer);
    }
};

struct Erosion_settings {
    gl::Buffer buffer;
    Erosion_data data = {
        .Kc = 0.060,
        .Ks = 0.00036,
        .Kd = 0.00006,
        .Ke = 0.003,
        .G = 1.0,
        .ENERGY_KEPT = 1.0,
        .Kalpha = 1.2f,
        .Kspeed = 8.25f,
        .d_t = 0.001,
    };
    void push_data() {
        buffer.push_data(data);
    }
    Erosion_settings() {
        buffer.binding = BIND_UNIFORM_EROSION;
        gl::gen_buffer(buffer);
    }
    ~Erosion_settings() {
        gl::del_buffer(buffer);
    }
};

// texture pairs for swapping
struct Tex_pair {
    gl::Texture t1; 
    gl::Texture t2; 

    GLuint r_bind;
    GLuint w_bind;

    u32 cntr = 0;
    void swap() {
        cntr++;
        if (cntr % 2) {
            t1.access = GL_WRITE_ONLY;
            gl::bind_texture(t1, w_bind);
            t2.access = GL_READ_ONLY;
            gl::bind_texture(t2, r_bind);
        } else {
            t1.access = GL_READ_ONLY;
            gl::bind_texture(t1, r_bind);
            t2.access = GL_WRITE_ONLY;
            gl::bind_texture(t2, w_bind);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    Tex_pair(GLenum access, GLuint width, GLuint height, GLuint bind_r, GLuint bind_w) {
        this->t1 = gl::Texture {
            .access = access,
            .width = width,
            .height = height
        };
        this->t2 = gl::Texture {
            .access = access,
            .width = width,
            .height = height
        };
        this->r_bind = bind_r;
        this->w_bind = bind_w;

        gl::gen_texture(t1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl::gen_texture(t2);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl::bind_texture(t1, r_bind);
        gl::bind_texture(t2, w_bind);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void delete_textures() {
        gl::delete_texture(t1);
        gl::delete_texture(t2);
    }
};

struct World_data {
    GLfloat time;
    Tex_pair heightmap;

    // hydraulic erosion
    Tex_pair flux;
    Tex_pair velocity;
    Tex_pair sediment;

    // thermal erosion
    Tex_pair thermal_c;
    Tex_pair thermal_d;

    gl::Buffer particle_buffer;
};

World_data gen_world_data(const GLuint width, const GLuint height) {
    Tex_pair heightmap(GL_READ_WRITE, width, height, BIND_HEIGHTMAP, BIND_WRITE_HEIGHTMAP);
    Tex_pair flux(GL_READ_WRITE, width, height, BIND_FLUXMAP, BIND_WRITE_FLUXMAP);
    Tex_pair velocity(GL_READ_WRITE, width, height, BIND_VELOCITYMAP, BIND_WRITE_VELOCITYMAP);
    Tex_pair sediment(GL_READ_WRITE, width, height, BIND_SEDIMENTMAP, BIND_WRITE_SEDIMENTMAP);

    // ------------- cross    flux for thermal erosion -----------
    Tex_pair thermal_c(GL_READ_WRITE, width, height, BIND_THERMALFLUX_C, BIND_WRITE_THERMALFLUX_C);
    // ------------- diagonal flux for thermal erosion -----------
    Tex_pair thermal_d(GL_READ_WRITE, width, height, BIND_THERMALFLUX_D, BIND_WRITE_THERMALFLUX_D);
    gl::Buffer particle_buffer {
        .binding = BIND_PARTICLE_BUFFER,
        .type = GL_SHADER_STORAGE_BUFFER
    };
    gl::gen_buffer(particle_buffer, PARTICLE_COUNT);

    return World_data {
        .heightmap = heightmap,
        .flux = flux,
        .velocity = velocity,
        .sediment = sediment,
        .thermal_c = thermal_c,
        .thermal_d = thermal_d,
        .particle_buffer = particle_buffer
    };
};

void delete_world_data(World_data& data) {
    data.heightmap.delete_textures();
    data.velocity.delete_textures();
    data.flux.delete_textures();
    data.sediment.delete_textures();
    data.thermal_c.delete_textures();
    data.thermal_d.delete_textures();
    // gl::del_buffer(data.mass_buffer);
}

void dispatch_rain(Compute_program& program, const World_data& data, Rain_settings& set) {
    program.use();
    program.set_uniform("time", data.time);
    // glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    // glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void dispatch_particle(
        Compute_program& program 
) {

}

void dispatch_erosion(
        Compute_program& hflux,
        Compute_program& heros,
        Compute_program& tflux,
        Compute_program& ttrans,
        Compute_program& sedim,
        Compute_program& smooth,
        World_data& data,
        Erosion_settings& set
    ) {
    auto run = [&](Compute_program& program, GLint layer = -1) {
        program.use();
        if (layer != -1) {
            program.set_uniform("t_layer", layer);
        }
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    };

    run(hflux);
    data.heightmap.swap();
    data.flux.swap();
    data.velocity.swap();

    run(heros);
    data.heightmap.swap();
    data.velocity.swap();
    data.sediment.swap();

    for (int i = 0; i < SED_LAYERS; i++) {
        run(tflux, i);
        data.thermal_c.swap();
        data.thermal_d.swap();

        run(ttrans, i);
        data.heightmap.swap();
    }

    run(sedim);
    data.heightmap.swap();
    data.sediment.swap();

    run(smooth);
    data.heightmap.swap();
}

struct Render_data {
    GLuint framebuffer;
    gl::Texture output_texture;
    gl::Buffer config_buff;

    float   prec = 0.35;
    bool    display_sediment = false;
    bool    debug_preview = false;
};

void delete_renderer(Render_data& data) {
    glDeleteFramebuffers(1, &data.framebuffer);
    gl::delete_texture(data.output_texture);
    gl::del_buffer(data.config_buff);
}

bool prepare_rendering(
        Render_data& rndr, 
        Compute_program& program, 
        World_data& data
    ) {
    program.use();

    glGenFramebuffers(1, &rndr.framebuffer);
    // output image rendered to framebuffer
    rndr.output_texture = {
        .target = GL_TEXTURE_2D, 
        .access = GL_WRITE_ONLY,
        .format = GL_RGBA32F, 
        .width = WINDOW_W,
        .height = WINDOW_H
    };
    gl::gen_texture(rndr.output_texture);
    // configuration
    struct Compute_config {
        alignas(sizeof(GLfloat)) GLfloat max_height    = MAX_HEIGHT;
        alignas(sizeof(GLint) * 2) ivec2 dims          = ivec2(NOISE_SIZE, NOISE_SIZE);
    } conf_buff;

    rndr.config_buff.binding = BIND_UNIFORM_CONFIG;
    gl::gen_buffer(rndr.config_buff);
    rndr.config_buff.push_data(conf_buff);
    program.bind_uniform_block("config", rndr.config_buff);
    gl::bind_texture(rndr.output_texture, BIND_DISPLAY_TEXTURE);
    glBindFramebuffer(GL_FRAMEBUFFER, rndr.framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, 
        GL_TEXTURE_2D, 
        rndr.output_texture.texture, 
        0
    );
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
            != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERR("FRAMEBUFFER INCOMPLETE!");
        return false;
    }    
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool dispatch_rendering(
        Compute_program& shader, 
        Render_data &rndr, 
        World_data& data,
        Erosion_settings& eros
    ) {
    using glm::perspective, glm::lookAt, glm::radians;
    shader.use();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    // TODO: change to a uniform buffer
    glm::mat4 mat_v = lookAt(
	    camera.pos, 
	    camera.pos + camera.dir, 
	    camera.up
	);
    glm::mat4 mat_p = perspective(
	    radians(FOV), 
	    ASPECT_RATIO, 
	    Z_NEAR, Z_FAR
	);

    shader.set_uniform("view", mat_v);
    shader.set_uniform("perspective", mat_p);
    shader.set_uniform("dir", camera.dir);
    shader.set_uniform("pos", camera.pos);
    shader.set_uniform("time", data.time);
    shader.set_uniform("prec", rndr.prec);
    shader.set_uniform("display_sediment", rndr.display_sediment);
    shader.set_uniform("sediment_max_cap", eros.data.Kc);
    shader.set_uniform("DEBUG_PREVIEW", rndr.debug_preview);

#ifdef LOW_RES_DIV3
    glDispatchCompute(WINDOW_W / (3 * WRKGRP_SIZE_X), WINDOW_H / (3 * WRKGRP_SIZE_Y), 1);
#else
    glDispatchCompute(WINDOW_W / WRKGRP_SIZE_X, WINDOW_H / WRKGRP_SIZE_Y, 1);
#endif

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    return true;
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    auto& imo = ImGui::GetIO();

	float xoffset = xpos - mouse_last.x;
	// reversed since y-coordinates range from bottom to top
	float yoffset = mouse_last.y - ypos; 
	mouse_last.x = xpos;
	mouse_last.y = ypos;

    if (imo.WantCaptureMouse || state.mouse_disabled) {
        return;
    }

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	camera.yaw += xoffset;
	camera.pitch += yoffset;
	if (camera.pitch > 89.f) {
		camera.pitch = 89.f;
	}
	if (camera.pitch < -89.f) {
		camera.pitch = -89.f;
	}
	vec3 direction;
	direction.x = cos(glm::radians(camera.yaw)) 
	    * cos(glm::radians(camera.pitch));
	direction.y = sin(glm::radians(camera.pitch));
	direction.z = sin(glm::radians(camera.yaw)) 
	    * cos(glm::radians(camera.pitch));
	camera.dir = normalize(direction);
}

void key_callback(GLFWwindow *window, 
        int key, int scancode, 
        int act, int mod) {

    auto& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }
    constexpr float speed_cap = 100.0f;
	float speed = 200.f * state.delta_frame * (camera.boost ? 8.f : 1.f);
	speed = speed > speed_cap ? speed_cap : speed;
    
	constexpr auto& gkey = glfwGetKey;
	if (gkey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.pos += speed * camera.dir;
	if (gkey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.pos -= speed * camera.dir;
	if (gkey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.pos -= 
			normalize(cross(camera.dir, camera.up)) * speed;
	if (gkey(window, GLFW_KEY_D) == GLFW_PRESS)
    	camera.pos += 
    		normalize(cross(camera.dir, camera.up)) * speed;
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera.boost = true;
	}
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
		camera.boost = false;
	}
	if (gkey(window, GLFW_KEY_LEFT_ALT)) {
	    auto curs = glfwGetInputMode(window, GLFW_CURSOR);
	    if (curs == GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
        state.mouse_disabled = !state.mouse_disabled;
	}
}

void gen_heightmap(
        Map_settings& map,
        Compute_program& program,
        World_data& world_data,
        Game_state& state
    ) {
    program.use();
    map.push_data();
    program.bind_uniform_block("map_cfg", map.buffer);
    // program.bind_storage_buffer("mass_data", world_data.mass_buffer);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}

void draw_ui(
        World_data& world,
        Rain_settings& rain,
        Erosion_settings& erosion,
        Render_data& render,
        Map_settings& map_settings,
        Compute_program& map_generator
    ) {
    // imgui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::Text("Camera pos: {%.2f %.2f %.2f}", 
        camera.pos.x, 
        camera.pos.y, 
        camera.pos.z
    );
    ImGui::Text("Camera dir: {%.2f %.2f %.2f}", 
        camera.dir.x, 
        camera.dir.y, 
        camera.dir.z
    );
    ImGui::Text("Frame time (ms): {%.2f}", state.frame_t);
    ImGui::Text("FPS: {%.2f}", 1000.0 / state.frame_t);
    ImGui::Text("Total erosion updates: {%lu}", state.erosion_steps);
    ImGui::Text("Avg erosion time: {%f}", state.erosion_mean_t);
    ImGui::Text("Total Time: {%f}", world.time);
    ImGui::End();

    ImGui::Begin("Settings");
    if (ImGui::Button(state.should_render ? "Disable Rendering" : "Start Rendering")) {
        state.should_render = !state.should_render;
    }
    ImGui::SameLine();
    if (ImGui::Button(state.should_erode ? "Stop Erosion" : "Erode")) {
        state.should_erode = !state.should_erode;
    }
    ImGui::SameLine();
    if (ImGui::Button(state.should_rain ? "Stop Raining" : "Rain")) {
        state.should_rain = !state.should_rain;
        rain.push_data();
    }
    ImGui::SeparatorText("Rain");
    ImGui::SliderFloat("Amount", &rain.data.amount, 0.0f, 1.0f, "%.5f");
    ImGui::SliderFloat("Bonus (%)", &rain.data.mountain_thresh, 0.0f, 1.0f);
    ImGui::SliderFloat("Bonus Amount", &rain.data.mountain_multip, 0.0f, 2.5f, "%.5f");
    ImGui::SliderInt("Tick period", &rain.data.period, 2, 10000);
    ImGui::SliderFloat("Drops", &rain.data.drops, 0.001, 0.1);

    ImGui::SeparatorText("Hydraulic Erosion");
    ImGui::SliderFloat("Energy Kept (%)", &erosion.data.ENERGY_KEPT, 0.998, 1.0, "%.5f");
    ImGui::SliderFloat("Capacity", &erosion.data.Kc, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Solubility", &erosion.data.Ks, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Deposition", &erosion.data.Kd, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Evaporation", &erosion.data.Ke, 0.0f, 1.00f);
    ImGui::SliderFloat("Gravitation", &erosion.data.G, 0.1f, 10.f);

    ImGui::SeparatorText("Thermal Erosion");
    ImGui::SliderAngle("Talus angle", &erosion.data.Kalpha, 0.00, 90.f);
    ImGui::SliderFloat("Erosion speed", &erosion.data.Kspeed, 0.01, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
    if (ImGui::Button("Set Erosion Settings")) {
        erosion.push_data();
    }
    ImGui::SeparatorText("General");
    ImGui::SliderFloat("Raymarching precision", &render.prec, 0.01f, 1.f);
    ImGui::Checkbox("Display sediment", &render.display_sediment);
    ImGui::Checkbox("Heightmap view", &render.debug_preview);
    ImGui::SliderFloat("Target_fps", &state.target_fps, 2.f, 120.f);
    ImGui::SliderFloat("Time step", &erosion.data.d_t, 0.0005f, 0.05f);
    ImGui::End();

    auto& map = map_settings.data;
    ImGui::Begin("Heightmap");
    ImGui::SliderFloat("Seed", &map.seed, 0.0f, 1e4);
    ImGui::SliderFloat("Height multiplier", &map.height_mult, 0.1f, 2.f);
    ImGui::SliderFloat("Persistence", &map.persistance, 0.0f, 1.f);
    ImGui::SliderFloat("Lacunarity", &map.lacunarity, 0.0f, 4.f);
    ImGui::SliderFloat("Scale", &map.scale, 0.0f, 0.01f, "%.5f");
    // ImGui::SliderFloat("Redistribution", &map.redistribution, 0.0f, 100.f);
    ImGui::SliderInt("Octaves", &map.octaves, 1, 10);

    ImGui::SeparatorText("Masking");
    ImGui::Checkbox("Circular", (bool*)&map.mask_round);
    ImGui::Checkbox("Exp", (bool*)&map.mask_exp);
    ImGui::Checkbox("Power3", (bool*)&map.mask_power);
    ImGui::Checkbox("Slope", (bool*)&map.mask_slope);
    
    if (ImGui::Button("Generate")) {
        bool old_erod = state.should_erode;
        state.should_erode = false;
        delete_world_data(world);
        world = gen_world_data(NOISE_SIZE, NOISE_SIZE);
        gen_heightmap(map_settings, map_generator, world, state);
        erosion.push_data();
        state.should_erode = old_erod;
    }
    ImGui::End();
    ImGui::Render();

}

int main(int argc, char* argv[]) { 
    srand(time(NULL));

    Uq_ptr<GLFWwindow, decltype(&destroy_window)> window(
        init_window(glm::uvec2{WINDOW_W, WINDOW_H}, "Game", &state.shader_error),
        destroy_window
    );

	glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window.get(), mouse_callback);
	glfwSetKeyCallback(window.get(), key_callback);

    assert(window.get() != nullptr);

    init_imgui(window.get());
    defer { destroy_imgui(); };

    Compute_program renderer(render_comput_file);

    Compute_program comput_map(noise_comput_file);
    /* Compute_program comput_rain(rain_comput_file);

    Compute_program comput_hydro_flux(erosion_hydro_flux_file);
    Compute_program comput_hydro_erosion(erosion_hydro_erosion_file);
    Compute_program comput_thermal_flux(erosion_thermal_flux_file);
    Compute_program comput_thermal_trans(erosion_thermal_transport_file);
    Compute_program comput_sediment(erosion_sediment_file);
    Compute_program comput_smooth(erosion_smooth_file); */

    Compute_program comput_particle(erosion_particle_file);

    World_data world_data = gen_world_data(NOISE_SIZE, NOISE_SIZE);
    defer{delete_world_data(world_data);};

    // ------------ noise generation -----------------
    Rain_settings rain_settings;
    rain_settings.push_data();
    // comput_rain.bind_uniform_block("settings", rain_settings.buffer);

    Erosion_settings erosion_settings;
    erosion_settings.push_data();

    // TODO: REFACTOR!!!
    /* comput_hydro_flux.bind_uniform_block("Erosion_data", erosion_settings.buffer);
    comput_hydro_erosion.bind_uniform_block("Erosion_data", erosion_settings.buffer);
    comput_sediment.bind_uniform_block("Erosion_data", erosion_settings.buffer);
    comput_thermal_flux.bind_uniform_block("Erosion_data", erosion_settings.buffer);
    comput_thermal_trans.bind_uniform_block("Erosion_data", erosion_settings.buffer); */

    Map_settings map_settings;
    gen_heightmap(map_settings, comput_map, world_data, state);

    // ---------- prepare textures for rendering  ---------------
    Render_data render_data;
    prepare_rendering(render_data, renderer, world_data);
    defer{delete_renderer(render_data);};

    while (!glfwWindowShouldClose(window.get()) && (!state.shader_error)) {
        glfwPollEvents();
        world_data.time = glfwGetTime();

        if(
            (world_data.time - state.last_frame + state.erosion_mean_t) >= (1 / state.target_fps) &&
            state.should_render
        ) {
            if (!dispatch_rendering(renderer, render_data, world_data, erosion_settings)) {
                return EXIT_FAILURE;
            }
            state.delta_frame = world_data.time - state.last_frame;
            state.frame_count += 1;
            if (world_data.time - state.last_frame_rounded >= 1.f) {
                state.frame_t = 1000.0 / (double)state.frame_count;
                state.frame_count = 0;
                state.last_frame_rounded += 1.f;
            }
            state.last_frame = glfwGetTime();
        }

        // ---------- erosion compute shader ------------
        if (state.should_erode) {
            state.erosion_steps++;
            /* if (state.should_rain) {
                if (!(state.erosion_steps % rain_settings.data.period)) {
                    dispatch_rain(comput_rain, world_data, rain_settings);
                }
            }  */
            float erosion_d_time = glfwGetTime();
            /* dispatch_erosion(
                comput_hydro_flux,
                comput_hydro_erosion, 
                comput_thermal_flux, 
                comput_thermal_trans, 
                comput_sediment, 
                comput_smooth, 
                world_data, 
                erosion_settings
            ); */
            // dispatch_particle();
            erosion_d_time = glfwGetTime() - erosion_d_time;
            state.erosion_time += erosion_d_time;
            // calculate average erosion update time
            if (!(state.erosion_steps % 100)) {
                state.erosion_mean_t = state.erosion_time / 100.f;
                state.erosion_time = 0;
            }
        }

        glBlitNamedFramebuffer(
            render_data.framebuffer, 0, 
            0, 0, WINDOW_W, WINDOW_H, 
            0, 0, WINDOW_W, WINDOW_H, 
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );

        draw_ui(
            world_data, 
            rain_settings, 
            erosion_settings, 
            render_data, 
            map_settings, 
            comput_map
        );
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
