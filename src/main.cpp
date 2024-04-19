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
constexpr float WATER_HEIGHT = 84.f;

constexpr float Z_NEAR = 0.1f;
constexpr float Z_FAR = 2048.f * 5;
constexpr float FOV = 90.f;
constexpr float ASPECT_RATIO = float(WINDOW_W) / WINDOW_H;

constexpr GLuint NOISE_SIZE = 384;

// shader filenames
constexpr auto noise_comput_file        = "heightmap.glsl";
constexpr auto rain_comput_file         = "rain.glsl";
constexpr auto render_comput_file       = "rendering.glsl";
constexpr auto erosion_erosion_file     = "erosion.glsl";
constexpr auto erosion_sediment_file    = "sediment_transport.glsl";
constexpr auto erosion_flux_file        = "water_flux.glsl";

static bool shader_error = false;

static float delta_frame = 0.f;
static float last_frame = 0.f;

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

struct World_data {
    GLfloat time;
    GLuint heightmap;
    GLuint out_heightmap;

    GLuint water_flux;
    GLuint out_water_flux;
    GLuint water_vel;
    GLuint out_water_vel;
};

void delete_world_textures(World_data& data) {
    glDeleteTextures(1, &data.heightmap);
    glDeleteTextures(1, &data.out_heightmap);


    glDeleteTextures(1, &data.water_flux);
    glDeleteTextures(1, &data.out_water_flux);

    glDeleteTextures(1, &data.water_vel);
    glDeleteTextures(1, &data.out_water_vel);
}

World_data gen_world_textures() {
    // ----------- noise generation ---------------
    auto gen_map_texture = [&](const GLuint width, const GLuint height) 
        -> GLuint {
        GLuint noise_texture;
        glGenTextures(1, &noise_texture);

        glBindTexture(GL_TEXTURE_2D, noise_texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA32F,
            width, height,
            0, 
            GL_RGBA, GL_FLOAT,
            nullptr
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        return noise_texture;
    };

    auto flux           = gen_map_texture(NOISE_SIZE, NOISE_SIZE);
    auto out_flux       = gen_map_texture(NOISE_SIZE, NOISE_SIZE);

    auto velocity       = gen_map_texture(NOISE_SIZE, NOISE_SIZE);
    auto out_velocity   = gen_map_texture(NOISE_SIZE, NOISE_SIZE);

    auto heightmap      = gen_map_texture(NOISE_SIZE, NOISE_SIZE);
    auto out_heightmap  = gen_map_texture(NOISE_SIZE, NOISE_SIZE);
    // ----------- water tex generation ---------------
    return World_data {
        .heightmap      = heightmap,
        .out_heightmap  = out_heightmap,
        .water_flux     = flux,
        .out_water_flux = out_flux,
        .water_vel      = velocity,
        .out_water_vel  = out_velocity
    };
}

void prepare_erosion(Compute_program& program, World_data& tex) {
    program.use();
    program.set_uniform("max_height", MAX_HEIGHT);
    glBindTexture(GL_TEXTURE_2D, tex.heightmap);
    glBindImageTexture(
        BIND_HEIGHTMAP, 
        tex.heightmap, 0, 
        GL_FALSE, 
        0, 
        GL_READ_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, tex.out_heightmap);
    glBindImageTexture(
        BIND_WRITE_HEIGHTMAP, 
        tex.out_heightmap, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, tex.water_flux);
    glBindImageTexture(
        BIND_FLUXMAP, 
        tex.water_flux, 0, 
        GL_FALSE, 
        0, 
        GL_READ_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, tex.out_water_flux);
    glBindImageTexture(
        BIND_WRITE_FLUXMAP, 
        tex.out_water_flux, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, tex.water_vel);
    glBindImageTexture(
        BIND_VELOCITYMAP, 
        tex.water_vel, 0, 
        GL_FALSE, 
        0, 
        GL_READ_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, tex.out_water_vel);
    glBindImageTexture(
        BIND_WRITE_VELOCITYMAP, 
        tex.out_water_vel, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, 0);
}

void dispatch_rain(Compute_program& program, const World_data& data) {
    program.use();
    program.set_uniform("time", data.time);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

//TODO: STOP COPYING, START SWAPPING BUFFERS 
void dispatch_erosion(
        Compute_program& program,
        World_data& data
    ) {
    program.use();
    program.set_uniform("d_t", 0.002f);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);

    glCopyImageSubData(
        data.out_heightmap,
        GL_TEXTURE_2D, 0,
        0, 0, 0,
        data.heightmap,
        GL_TEXTURE_2D, 0,
        0, 0, 0,
        NOISE_SIZE, NOISE_SIZE, 1
    );

    glCopyImageSubData(
        data.out_water_flux,
        GL_TEXTURE_2D, 0,
        0, 0, 0,
        data.water_flux,
        GL_TEXTURE_2D, 0,
        0, 0, 0,
        NOISE_SIZE, NOISE_SIZE, 1
    );

    glCopyImageSubData(
        data.out_water_vel,
        GL_TEXTURE_2D, 0,
        0, 0, 0,
        data.water_vel,
        GL_TEXTURE_2D, 0,
        0, 0, 0,
        NOISE_SIZE, NOISE_SIZE, 1
    );

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

struct Render_data {
    GLuint framebuffer;
    GLuint output_texture;
    GLuint config_buffer;
};

void delete_renderer(Render_data& data) {
    glDeleteFramebuffers(1, &data.framebuffer);
    glDeleteTextures(1, &data.output_texture);
    glDeleteBuffers(1, &data.config_buffer);
}

bool prepare_rendering(
        Render_data& data, 
        Compute_program& program, 
        World_data& textures
    ) {
    program.use();

    glGenFramebuffers(1, &data.framebuffer);
    // output image rendered to framebuffer
    glGenTextures(1, &data.output_texture);
    glBindTexture(GL_TEXTURE_2D, data.output_texture);
    glTexImage2D(
        GL_TEXTURE_2D, 
        0, 
        GL_RGBA32F, 
        WINDOW_W, WINDOW_H, 
        0, 
        GL_RGBA, GL_FLOAT, 
        nullptr
    );
    // configuration
    constexpr struct Compute_config {
        alignas(16) float max_height    = MAX_HEIGHT;
        alignas(16) ivec2 dims          = ivec2(NOISE_SIZE, NOISE_SIZE);
    } conf_buff;

    glGenBuffers(1, &data.config_buffer);

    glBindBuffer(GL_UNIFORM_BUFFER, data.config_buffer);
    glBufferData(
        GL_UNIFORM_BUFFER, 
        sizeof(conf_buff), &conf_buff, 
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    program.bind_uniform_block("config", data.config_buffer);

    glBindTexture(GL_TEXTURE_2D, textures.heightmap);
    glBindImageTexture(
        BIND_HEIGHTMAP, 
        textures.heightmap, 0, 
        GL_FALSE, 
        0, 
        GL_READ_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, data.output_texture);
    glBindImageTexture(
        BIND_DISPLAY_TEXTURE, 
        data.output_texture, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glBindFramebuffer(GL_FRAMEBUFFER, data.framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, 
        GL_TEXTURE_2D, 
        data.output_texture, 
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
        World_data& data
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

    glDispatchCompute(WINDOW_W / WRKGRP_SIZE_X, WINDOW_H / WRKGRP_SIZE_Y, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glClear(GL_COLOR_BUFFER_BIT);

    glBlitNamedFramebuffer(
        rndr.framebuffer, 0, 
        0, 0, WINDOW_W, WINDOW_H, 
        0, 0, WINDOW_W, WINDOW_H, 
        GL_COLOR_BUFFER_BIT, GL_NEAREST
    );
    return true;
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
	float xoffset = xpos - mouse_last.x;
	// reversed since y-coordinates range from bottom to top
	float yoffset = mouse_last.y - ypos; 
	mouse_last.x = xpos;
	mouse_last.y = ypos;

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
    constexpr float speed_cap = 100.0f;
	float speed = 200.f * delta_frame * (camera.boost ? 8.f : 1.f);
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
}

int main(int argc, char* argv[]) { 
    uint seed;
    srand(time(NULL));
    if (argc == 2) {
        seed = strtoul(argv[1], NULL, 10);
        LOG_DBG("SEED: %u", seed);
    } else {
        seed = rand();
    }

    Uq_ptr<GLFWwindow, decltype(&destroy_window)> window(
        init_window(glm::uvec2{WINDOW_W, WINDOW_H}, "Game", &shader_error),
        destroy_window
    );

	glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window.get(), mouse_callback);
	glfwSetKeyCallback(window.get(), key_callback);

    assert(window.get() != nullptr);

    init_imgui(window.get());
    defer { destroy_imgui(); };

    Compute_program heightmap_shader(noise_comput_file);
    Compute_program heightmap_rain(rain_comput_file);
    Compute_program render_shader(render_comput_file);

    Compute_program erosion_erosion(erosion_erosion_file);
    Compute_program erosion_sediment(erosion_sediment_file);
    Compute_program erosion_flux(erosion_flux_file);

    // textures 
    World_data world_data = gen_world_textures();
    defer{ delete_world_textures(world_data);};

    // ------------ noise generation -----------------
    heightmap_shader.use();
    glBindTexture(GL_TEXTURE_2D, world_data.heightmap);
    glBindImageTexture(
        BIND_HEIGHTMAP, 
        world_data.heightmap, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, world_data.water_flux);
    glBindImageTexture(
        BIND_FLUXMAP, 
        world_data.water_flux, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glBindTexture(GL_TEXTURE_2D, world_data.water_vel);
    glBindImageTexture(
        BIND_VELOCITYMAP, 
        world_data.water_vel, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    heightmap_shader.set_uniform("height_scale", MAX_HEIGHT);
    heightmap_shader.set_uniform("water_lvl", WATER_HEIGHT);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);

    // ---------- prepare textures for rendering  ---------------
    Render_data render_data;
    prepare_rendering(render_data, render_shader, world_data);
    defer{delete_renderer(render_data);};
    prepare_erosion(heightmap_rain, world_data);

    prepare_erosion(erosion_flux, world_data);
    prepare_erosion(erosion_erosion, world_data);
    prepare_erosion(erosion_sediment, world_data);

    u32 frame_count = 0;
    float last_frame_rounded = 0.0;
    double frame_t = 0.0;

    const u32 max_rain_steps = 900000;
    u32 erosion_steps = 0;

    float mean_erosion_t = 0.f;

    while (!glfwWindowShouldClose(window.get()) && (!shader_error)) {
        glfwPollEvents();
        world_data.time = glfwGetTime();

        if ((world_data.time - last_frame + mean_erosion_t) >= (1/40.f)) {
            if (!dispatch_rendering(render_shader, render_data, world_data)) {
                return EXIT_FAILURE;
            }
            delta_frame = world_data.time - last_frame;
            frame_count += 1;
            if (world_data.time - last_frame_rounded >= 1.f) {
                frame_t = 1000.0 / (double)frame_count;
                frame_count = 0;
                last_frame_rounded += 1.f;
            }
            last_frame = world_data.time;
        }

        // ---------- erosion compute shader ------------
        if (erosion_steps < max_rain_steps) {
            if (!(erosion_steps % 1048)) {
                dispatch_rain(heightmap_rain, world_data);
            }
        } 
        dispatch_erosion(erosion_flux, world_data);
        dispatch_erosion(erosion_erosion, world_data);
        dispatch_erosion(erosion_sediment, world_data);

        mean_erosion_t = glfwGetTime() / erosion_steps;
        erosion_steps++;

        // imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Info");
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
        ImGui::Text("Frame time (ms): {%.2f}", frame_t);
        ImGui::Text("FPS: {%.2f}", 1000.0 / frame_t);
        ImGui::Text("Erosion updates: {%lu}", erosion_steps);
        ImGui::Text("Mean erosion time: {%f}", mean_erosion_t);
        ImGui::Text("Total Time: {%f}", world_data.time);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
