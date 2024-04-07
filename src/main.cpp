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

constexpr u32 WINDOW_W = 1920;
constexpr u32 WINDOW_H = 1080;

constexpr float Z_NEAR = 0.1f;
constexpr float Z_FAR = 2048.f;
constexpr float FOV = 90.f;
constexpr float ASPECT_RATIO = float(WINDOW_W) / WINDOW_H;

constexpr auto compute_noise_file = "compute.glsl";
constexpr auto compute_verts_file = "compute_vertices.glsl";
constexpr auto simplex_noise_file = "simplex_noise.glsl";

// ----------------------

static float delta_time = 0.f;
static float last_frame = 0.f;

using glm::normalize, glm::cross;
using glm::vec2, glm::vec3, glm::ivec2, glm::ivec3;
using glm::vec4, glm::ivec4;

// mouse position
static vec2 mouse_last;
static struct Camera {
	vec3 pos    = vec3(0.f, 38.f, 0.f);
	vec3 dir    = vec3(0.f, 38.f, -1.f);

	vec3 _def_dir = normalize(pos - dir);
	static constexpr float speed = 0.05f;

	vec3 right  = normalize(cross(vec3(0,1,0), _def_dir));
	vec3 up     = cross(_def_dir, right);

	float yaw   = -90.f;
	float pitch = 0.f;

	bool boost  = false;	
} camera;

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
    constexpr float speed_cap = 4.0f;
	float speed = 20.f * delta_time * (camera.boost ? 4.f : 1.f);
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
        init_window(glm::uvec2{WINDOW_W, WINDOW_H}, "Game"),
        destroy_window
    );

	glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window.get(), mouse_callback);
	glfwSetKeyCallback(window.get(), key_callback);

    assert(window.get() != nullptr);

    init_imgui(window.get());
    defer { destroy_imgui(); };

    Compute_program compute_noise(compute_noise_file);
    Compute_program compute_output(compute_verts_file);

    // ----------- noise generation ---------------
    constexpr GLuint noise_size = 4096;
    GLuint noise_buffer;
    glGenBuffers(1, &noise_buffer);
    defer { glDeleteBuffers(1, &noise_buffer); };

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, noise_buffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER, 
        sizeof(float) * 4 * noise_size  * noise_size, nullptr, 
        GL_STATIC_DRAW
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, noise_buffer);

    GLuint noise_texture;
    glGenTextures(1, &noise_texture);
    defer { glDeleteTextures(1, &noise_texture); };

    glBindTexture(GL_TEXTURE_2D, noise_texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA32F,
        noise_size, noise_size,
        0, 
        GL_RGBA, GL_FLOAT,
        nullptr
    );
    glBindImageTexture(
        0, 
        noise_texture, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // ----------- water tex generation ---------------
    GLuint water_buffer;
    glGenBuffers(1, &water_buffer);
    defer { glDeleteBuffers(1, &water_buffer); };

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, water_buffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER, 
        WINDOW_W * sizeof(float) * WINDOW_H, nullptr, 
        GL_STATIC_DRAW
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, water_buffer);

    GLuint water_tex;
    glGenTextures(1, &water_tex);
    defer { glDeleteTextures(1, &water_tex); };

    glBindTexture(GL_TEXTURE_2D, water_tex);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R32F,
        noise_size, noise_size,
        0, 
        GL_RED, GL_FLOAT,
        nullptr
    );
    glBindImageTexture(
        1, 
        water_tex, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_R32F
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Set texture wrapping behavior
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    compute_noise.use();
    glDispatchCompute(noise_size / 8, noise_size / 8, 1);

    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    defer { glDeleteFramebuffers(1, &framebuffer); };

    // ---------- output framebuffer  ---------------

    constexpr u32 group_size = 8;
    constexpr u32 chunk_size = 8;
    constexpr u32 total_size = group_size * chunk_size;

    compute_output.use();

    // configuration
    struct Compute_config {
        alignas(16) ivec2 dims          = ivec2(noise_size, noise_size);
        /* alignas(16) vec4 sky_color      = vec4(0.45, 0.716, 0.914, 1);
        alignas(16) vec4 water_color    = vec4(0.15, 0.216, 0.614, 1); */
    } conf_buff;

    GLuint ubo_config;
    glGenBuffers(1, &ubo_config);
    defer { glDeleteBuffers(1, &ubo_config); };

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_config);
    glBufferData(
        GL_UNIFORM_BUFFER, 
        sizeof(conf_buff), &conf_buff, 
        GL_STATIC_DRAW
    );
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    compute_output.ub_bind("config", ubo_config);

    using glm::lookAt, glm::perspective, glm::radians;

    glBindTexture(GL_TEXTURE_2D, noise_texture);
    glTexImage2D(
        GL_TEXTURE_2D, 
        0, 
        GL_RGBA32F, 
        noise_size, noise_size, 
        0, 
        GL_RGBA, GL_FLOAT, 
        nullptr
    );
    glBindImageTexture(
        2, 
        noise_texture, 0, 
        GL_FALSE, 
        0, 
        GL_READ_ONLY, 
        GL_RGBA32F
    );

    glBindTexture(GL_TEXTURE_2D, water_tex);
    glTexImage2D(
        GL_TEXTURE_2D, 
        0, 
        GL_R32F, 
        noise_size, noise_size, 
        0, 
        GL_RED, GL_FLOAT, 
        nullptr
    );
    glBindImageTexture(
        4, 
        water_tex, 0, 
        GL_FALSE, 
        0, 
        GL_READ_ONLY, 
        GL_R32F
    );

    GLuint output_buffer;
    glGenBuffers(1, &output_buffer);
    defer { glDeleteBuffers(1, &output_buffer); };

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, output_buffer);
    glBufferData(
        GL_SHADER_STORAGE_BUFFER, 
        WINDOW_W * sizeof(float) * WINDOW_H, nullptr, 
        GL_STATIC_DRAW
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, output_buffer);

    GLuint output_texture;
    glGenTextures(1, &output_texture);
    defer { glDeleteTextures(1, &output_texture); };

    glBindTexture(GL_TEXTURE_2D, output_texture);
    glTexImage2D(
        GL_TEXTURE_2D, 
        0, 
        GL_RGBA32F, 
        WINDOW_W, WINDOW_H, 
        0, 
        GL_RGBA, GL_FLOAT, 
        nullptr
    );
    glBindImageTexture(
        3, 
        output_texture, 0, 
        GL_FALSE, 
        0, 
        GL_WRITE_ONLY, 
        GL_RGBA32F
    );

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, 
        GL_TEXTURE_2D, 
        output_texture, 
        0
    );
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
            != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERR("FRAMEBUFFER INCOMPLETE!");
        return EXIT_FAILURE;
    }    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDispatchCompute(WINDOW_W / 8, WINDOW_H / 8, 1);

    u32 frame_count = 0;
    float last_frame_rounded = 0.0;
    double frame_time = 0.0;
    while (!glfwWindowShouldClose(window.get())) {
        frame_count += 1;

        float current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        if (current_frame - last_frame_rounded >= 1.f) {
            frame_time = 1000.0 / (double)frame_count;
            frame_count = 0;
            last_frame_rounded += 1.f;
        }
        last_frame = current_frame;

        glfwPollEvents();

        compute_output.use();
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

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

	    compute_output.set_uniform("view", mat_v);
	    compute_output.set_uniform("perspective", mat_p);

	    compute_output.set_uniform("dir", camera.dir);
	    compute_output.set_uniform("pos", camera.pos);
	    compute_output.set_uniform("time", current_frame);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
                != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERR("FRAMEBUFFER INCOMPLETE!");
            return EXIT_FAILURE;
        }    
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDispatchCompute(WINDOW_W / 8, WINDOW_H / 8, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glClear(GL_COLOR_BUFFER_BIT);

        glBlitNamedFramebuffer(
            framebuffer, 0, 
            0, 0, WINDOW_W, WINDOW_H, 
            0, 0, WINDOW_W, WINDOW_H, 
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );
        // imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Info");
        ImGui::Text("camera pos: {%.2f %.2f %.2f}", 
            camera.pos.x, 
            camera.pos.y, 
            camera.pos.z
        );
        ImGui::Text("camera dir: {%.2f %.2f %.2f}", 
            camera.dir.x, 
            camera.dir.y, 
            camera.dir.z
        );
        ImGui::Text("frame time (ms): {%.2f}", frame_time);
        ImGui::Text("fps: {%.2f}", 1000.0 / frame_time);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
