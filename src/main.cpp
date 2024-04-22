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
constexpr float Z_FAR = 2048.f * 5;
constexpr float FOV = 90.f;
constexpr float ASPECT_RATIO = float(WINDOW_W) / WINDOW_H;

constexpr GLuint NOISE_SIZE = 864;

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

struct Rain_settings {
    float amount = 0.01f;
    float mountain_thresh = 0.65f;
    float mountain_multip = 0.05f;
    int period = 48;
};

struct Erosion_settings {
    float Kc = 0.001;
    float Ks = 0.001;
    float Kd = 0.001;
    float Ke = 0.05;
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

void delete_world_data(World_data& data) {
    glDeleteTextures(1, &data.heightmap.t1.texture);
    glDeleteTextures(1, &data.heightmap.t2.texture);

    glDeleteTextures(1, &data.flux.t1.texture);
    glDeleteTextures(1, &data.flux.t2.texture);

    glDeleteTextures(1, &data.velocity.t1.texture);
    glDeleteTextures(1, &data.velocity.t2.texture);

    glDeleteTextures(1, &data.thermal_c.t1.texture);
    glDeleteTextures(1, &data.thermal_c.t2.texture);

    glDeleteTextures(1, &data.thermal_d.t1.texture);
    glDeleteTextures(1, &data.thermal_d.t2.texture);
}

World_data gen_world_data(const GLuint width, const GLuint height) {
    // ------------- heightmap storing terrain + water + sediment data
    Tex_pair heightmap = {
        .t1 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
        .t2 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
    };
    heightmap.r_bind = BIND_HEIGHTMAP;
    heightmap.w_bind = BIND_WRITE_HEIGHTMAP;

    gl::gen_texture(heightmap.t1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    gl::gen_texture(heightmap.t2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ------------- water flux for hydraulic erosion ------------
    Tex_pair flux = {
        .t1 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
        .t2 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
    };
    flux.r_bind = BIND_FLUXMAP;
    flux.w_bind = BIND_WRITE_FLUXMAP;

    gl::gen_texture(flux.t1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    gl::gen_texture(flux.t2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ------------- water velocity for hydraulic erosion --------
    Tex_pair velocity = {
        .t1 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
        .t2 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
    };
    velocity.r_bind = BIND_VELOCITYMAP;
    velocity.w_bind = BIND_WRITE_VELOCITYMAP;

    gl::gen_texture(velocity.t1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    gl::gen_texture(velocity.t2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ------------- cross    flux for thermal erosion -----------
    Tex_pair thermal_c = {
        .t1 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
        .t2 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
    };
    thermal_c.r_bind = BIND_THERMALFLUX_C;
    thermal_c.w_bind = BIND_WRITE_THERMALFLUX_C;

    gl::gen_texture(thermal_c.t1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    gl::gen_texture(thermal_c.t2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // ------------- diagonal flux for thermal erosion -----------
    Tex_pair thermal_d = {
        .t1 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
        .t2 = gl::Texture {
            .access = GL_READ_WRITE,
            .width = width,
            .height = height
        },
    };
    thermal_d.r_bind = BIND_THERMALFLUX_D;
    thermal_d.w_bind = BIND_WRITE_THERMALFLUX_D;

    gl::gen_texture(thermal_d.t1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    gl::gen_texture(thermal_d.t2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl::bind_texture(heightmap.t1, BIND_HEIGHTMAP);
    gl::bind_texture(heightmap.t2, BIND_WRITE_HEIGHTMAP);

    gl::bind_texture(flux.t1, BIND_FLUXMAP);
    gl::bind_texture(flux.t2, BIND_WRITE_FLUXMAP);
    gl::bind_texture(velocity.t1, BIND_VELOCITYMAP);
    gl::bind_texture(velocity.t2, BIND_WRITE_VELOCITYMAP);

    gl::bind_texture(thermal_c.t1, BIND_THERMALFLUX_C);
    gl::bind_texture(thermal_c.t2, BIND_WRITE_THERMALFLUX_C);

    gl::bind_texture(thermal_d.t1, BIND_THERMALFLUX_D);
    gl::bind_texture(thermal_d.t2, BIND_WRITE_THERMALFLUX_D);

    glBindTexture(GL_TEXTURE_2D, 0);

    return World_data {
        .heightmap = heightmap,
        .flux = flux,
        .velocity = velocity,
        .thermal_c = thermal_c,
        .thermal_d = thermal_d
    };
};

void dispatch_rain(Compute_program& program, const World_data& data, Rain_settings& set) {
    program.use();
    program.set_uniform("time", data.time);
    program.set_uniform("rain_amount", set.amount);
    program.set_uniform("MOUNT_HGH", set.mountain_thresh);
    program.set_uniform("mount_mtp", set.mountain_multip);
    program.set_uniform("max_height", MAX_HEIGHT);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void dispatch_erosion(
        Compute_program& program,
        World_data& data,
        Erosion_settings& set
    ) {
    program.use();
    program.set_uniform("max_height", MAX_HEIGHT);
    program.set_uniform("d_t", 0.002f);
    program.set_uniform("Kc", set.Kc);
    program.set_uniform("Ks", set.Ks);
    program.set_uniform("Kd", set.Kd);
    program.set_uniform("Ke", set.Ke);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);

    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    // swapping textures
    data.heightmap.swap();
    data.flux.swap();
    data.velocity.swap();
}

struct Render_data {
    GLuint framebuffer;
    gl::Texture output_texture;
    GLuint config_buffer;
};

void delete_renderer(Render_data& data) {
    glDeleteFramebuffers(1, &data.framebuffer);
    glDeleteTextures(1, &data.output_texture.texture);
    glDeleteBuffers(1, &data.config_buffer);
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

    glGenBuffers(1, &rndr.config_buffer);

    glBindBuffer(GL_UNIFORM_BUFFER, rndr.config_buffer);
    glBufferData(
        GL_UNIFORM_BUFFER, 
        sizeof(conf_buff), &conf_buff, 
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    program.bind_uniform_block("config", rndr.config_buffer);
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
	if (gkey(window, GLFW_KEY_ENTER)) {
	    auto curs = glfwGetInputMode(window, GLFW_CURSOR);
	    if (curs == GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
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

    World_data world_data = gen_world_data(NOISE_SIZE, NOISE_SIZE);
    defer{ delete_world_data(world_data);};

    // ------------ noise generation -----------------
    heightmap_shader.use();
    heightmap_shader.set_uniform("height_scale", MAX_HEIGHT);
    heightmap_shader.set_uniform("water_lvl", WATER_HEIGHT);
    gl::bind_texture(world_data.heightmap.t1, BIND_HEIGHTMAP);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);

    // ---------- prepare textures for rendering  ---------------
    Render_data render_data;
    prepare_rendering(render_data, render_shader, world_data);
    defer{delete_renderer(render_data);};

    u32 frame_count = 0;
    float last_frame_rounded = 0.0;
    double frame_t = 0.0;

    //const u32 max_rain_steps = 900000;
    bool should_rain = true;
    u32 erosion_steps = 0;

    float mean_erosion_t = 0.f;
    Rain_settings rain_settings;
    Erosion_settings erosion_settings;

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
        if (should_rain) {
            if (!(erosion_steps % rain_settings.period)) {
                dispatch_rain(heightmap_rain, world_data, rain_settings);
            }
        } 
        dispatch_erosion(erosion_flux, world_data, erosion_settings);
        dispatch_erosion(erosion_erosion, world_data, erosion_settings);
        dispatch_erosion(erosion_sediment, world_data, erosion_settings);

        mean_erosion_t = glfwGetTime() / erosion_steps;
        erosion_steps++;
        // imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Debug");
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

        ImGui::Begin("Settings");
        ImGui::SeparatorText("Rain");
        if (ImGui::Button("Rain")) {
            should_rain = !should_rain;
        }
        ImGui::SliderFloat("Amount", &rain_settings.amount, 0.0f, 1.0f);
        ImGui::SliderFloat("Bonus %", &rain_settings.mountain_thresh, 0.0f, 1.0f);
        ImGui::SliderFloat("Bonus Amount", &rain_settings.mountain_multip, 0.0f, 2.5f);
        ImGui::SliderInt("Tick period", &rain_settings.period, 2, 1000);

        ImGui::SeparatorText("Erosion");
        ImGui::SliderFloat("Capacity", &erosion_settings.Kc, 0.0001f, 0.1f);
        ImGui::SliderFloat("Dissolving", &erosion_settings.Ks, 0.0001f, 0.1f);
        ImGui::SliderFloat("Deposition", &erosion_settings.Kd, 0.0001f, 0.1f);
        ImGui::SliderFloat("Evaporation", &erosion_settings.Ke, 0.0f, 1.f);

        glBlitNamedFramebuffer(
            render_data.framebuffer, 0, 
            0, 0, WINDOW_W, WINDOW_H, 
            0, 0, WINDOW_W, WINDOW_H, 
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
