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

constexpr u32 WINDOW_W = 1920;
constexpr u32 WINDOW_H = 1080;

constexpr float MAX_HEIGHT = 256.f;
constexpr float WATER_HEIGHT = 96.f;

constexpr float Z_NEAR = 0.1f;
constexpr float Z_FAR = 2048.f;
constexpr float FOV = 90.f;
constexpr float ASPECT_RATIO = float(WINDOW_W) / WINDOW_H;

constexpr GLuint NOISE_SIZE = 864;

// shader filenames
constexpr auto noise_comput_file            = "heightmap.glsl";
constexpr auto rain_comput_file             = "rain.glsl";
constexpr auto render_comput_file           = "rendering.glsl";

constexpr auto erosion_hydro_flux_file      = "hydro_flux.glsl";
constexpr auto erosion_hydro_erosion_file   = "hydro_erosion.glsl";
constexpr auto erosion_thermal_flux_file    = "thermal_erosion.glsl";
constexpr auto erosion_sediment_file        = "sediment_transport.glsl";

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
    float amount = 0.00001f;
    float mountain_thresh = 0.55f;
    float mountain_multip = 0.005f;
    int period = 2;
    float drops = 0.02f;
};

struct Map_settings {
    gl::Uniform_buffer buffer;
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
        buffer.push_data(this->data, GL_STATIC_DRAW);
    }
    Map_settings() {
        gl::gen_uniform_buffer(this->buffer);
    }
    ~Map_settings() {
        gl::delete_uniform_buffer(this->buffer);
    }
};

struct Erosion_settings {
    GLfloat Kc = 0.015;
    GLfloat Ks = 0.015;
    GLfloat Kd = 0.011;
    GLfloat Ke = 0.05;
    GLfloat G = 9.81;
    GLfloat ENERGY_LOSS = 0.99985;

    GLfloat Kalpha = 1.2f;
    GLfloat Kspeed = 0.011f;

    // INCREASING TIMESTEP TOO MUCH WILL BREAK STABILITY
    GLfloat d_t = 0.003;
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        gl::gen_texture(t2);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

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

    // thermal erosion
    Tex_pair thermal_c;
    Tex_pair thermal_d;
};

World_data gen_world_data(const GLuint width, const GLuint height) {
    Tex_pair heightmap(GL_READ_WRITE, width, height, BIND_HEIGHTMAP, BIND_WRITE_HEIGHTMAP);
    Tex_pair flux(GL_READ_WRITE, width, height, BIND_FLUXMAP, BIND_WRITE_FLUXMAP);
    Tex_pair velocity(GL_READ_WRITE, width, height, BIND_VELOCITYMAP, BIND_WRITE_VELOCITYMAP);

    // ------------- cross    flux for thermal erosion -----------
    Tex_pair thermal_c(GL_READ_WRITE, width, height, BIND_THERMALFLUX_C, BIND_WRITE_THERMALFLUX_C);
    // ------------- diagonal flux for thermal erosion -----------
    Tex_pair thermal_d(GL_READ_WRITE, width, height, BIND_THERMALFLUX_D, BIND_WRITE_THERMALFLUX_D);

    return World_data {
        .heightmap = heightmap,
        .flux = flux,
        .velocity = velocity,
        .thermal_c = thermal_c,
        .thermal_d = thermal_d
    };
};

void delete_world_data(World_data& data) {
    data.heightmap.delete_textures();
    data.velocity.delete_textures();
    data.flux.delete_textures();
    data.thermal_c.delete_textures();
    data.thermal_d.delete_textures();
}

void dispatch_rain(Compute_program& program, const World_data& data, Rain_settings& set) {
    program.use();
    program.set_uniform("time", data.time);
    program.set_uniform("rain_amount", set.amount);
    program.set_uniform("MOUNT_HGH", set.mountain_thresh);
    program.set_uniform("mount_mtp", set.mountain_multip);
    program.set_uniform("max_height", MAX_HEIGHT);
    program.set_uniform("drops", set.drops);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void dispatch_erosion(
        Compute_program& hflux,
        Compute_program& heros,
        Compute_program& tflux,
        Compute_program& sedim,
        World_data& data,
        Erosion_settings& set
    ) {

    float d_t = set.d_t * rand() / (float)RAND_MAX;
    auto run = [&](Compute_program& program) {
        program.use();
        program.set_uniform("max_height", MAX_HEIGHT);
        program.set_uniform("ENERGY_LOSS", set.ENERGY_LOSS);
        program.set_uniform("d_t", d_t);
        program.set_uniform("Kc", set.Kc);
        program.set_uniform("Ks", set.Ks);
        program.set_uniform("Kd", set.Kd);
        program.set_uniform("Ke", set.Ke);
        program.set_uniform("G", set.G);

        program.set_uniform("Kalpha", set.Kalpha);
        program.set_uniform("Kspeed", set.Kspeed);

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    };

    run(hflux);
    data.heightmap.swap();
    data.flux.swap();
    data.velocity.swap();

    run(heros);
    data.heightmap.swap();
    data.velocity.swap();

    run(tflux);
    data.thermal_c.swap();
    data.thermal_d.swap();

    run(sedim);
    data.heightmap.swap();
}

struct Render_data {
    GLuint framebuffer;
    gl::Texture output_texture;
    gl::Uniform_buffer config_buff;

    float   prec = 0.35;
    bool    display_sediment = false;
};

void delete_renderer(Render_data& data) {
    glDeleteFramebuffers(1, &data.framebuffer);
    gl::delete_texture(data.output_texture);
    gl::delete_uniform_buffer(data.config_buff);
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
    constexpr struct Compute_config {
        alignas(sizeof(GLfloat)) GLfloat max_height    = MAX_HEIGHT;
        alignas(sizeof(GLint) * 2) ivec2 dims          = ivec2(NOISE_SIZE, NOISE_SIZE);
    } conf_buff;

    gl::gen_uniform_buffer(rndr.config_buff);
    rndr.config_buff.push_data(conf_buff);
    program.bind_uniform_block("config", rndr.config_buff.ubo);
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
    shader.set_uniform("sediment_max_cap", eros.Kc);

    glDispatchCompute(WINDOW_W / (WRKGRP_SIZE_X), WINDOW_H / (WRKGRP_SIZE_Y), 1);

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
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    map.push_data();
    program.bind_uniform_block("map_cfg", map.buffer.ubo);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
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
    }
    ImGui::SeparatorText("Rain");
    ImGui::SliderFloat("Amount", &rain.amount, 0.0f, 1.0f, "%.5f");
    ImGui::SliderFloat("Bonus (%)", &rain.mountain_thresh, 0.0f, 1.0f);
    ImGui::SliderFloat("Bonus Amount", &rain.mountain_multip, 0.0f, 2.5f, "%.5f");
    ImGui::SliderInt("Tick period", &rain.period, 2, 10000);
    ImGui::SliderFloat("Drops", &rain.drops, 0.001, 0.1);

    ImGui::SeparatorText("Hydraulic Erosion");
    ImGui::SliderFloat("Energy Kept (%)", &erosion.ENERGY_LOSS, 0.998, 1.0, "%.5f");
    ImGui::SliderFloat("Capacity", &erosion.Kc, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Solubility", &erosion.Ks, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Deposition", &erosion.Kd, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Evaporation", &erosion.Ke, 0.0f, 1.00f);
    ImGui::SliderFloat("Gravitation", &erosion.G, 0.1f, 10.f);

    ImGui::SeparatorText("Thermal Erosion");
    ImGui::SliderAngle("Talus angle", &erosion.Kalpha, 0.00, 90.f);
    ImGui::SliderFloat("Erosion speed", &erosion.Kspeed, 0.01, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);

    ImGui::SeparatorText("General");
    ImGui::SliderFloat("Raymarching precision", &render.prec, 0.01f, 1.f);
    ImGui::Checkbox("Display sediment", &render.display_sediment);
    ImGui::SliderFloat("Target_fps", &state.target_fps, 2.f, 120.f);
    ImGui::SliderFloat("Time step", &erosion.d_t, 0.0005f, 0.05f);
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
    Compute_program comput_rain(rain_comput_file);

    Compute_program comput_hydro_flux(erosion_hydro_flux_file);
    Compute_program comput_hydro_erosion(erosion_hydro_erosion_file);
    Compute_program comput_thermal_flux(erosion_thermal_flux_file);
    Compute_program comput_sediment(erosion_sediment_file);

    World_data world_data = gen_world_data(NOISE_SIZE, NOISE_SIZE);
    defer{delete_world_data(world_data);};

    // ------------ noise generation -----------------
    Rain_settings rain_settings;
    Erosion_settings erosion_settings;

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
            if (state.should_rain) {
                if (!(state.erosion_steps % rain_settings.period)) {
                    dispatch_rain(comput_rain, world_data, rain_settings);
                }
            } 
            float erosion_d_time = glfwGetTime();
            dispatch_erosion(
                comput_hydro_flux,
                comput_hydro_erosion, 
                comput_sediment, 
                comput_thermal_flux, 
                world_data, 
                erosion_settings
            );
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
