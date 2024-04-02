#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "utils.hpp"

constexpr u32 WINDOW_W = 1280;
constexpr u32 WINDOW_H = 720;

constexpr float Z_NEAR = 0.1f;
constexpr float Z_FAR = 128.f;
constexpr float FOV = 90.f;
constexpr float ASPECT_RATIO = float(WINDOW_W) / WINDOW_H;

constexpr auto vert_shader_file = "../glsl/vert.glsl";
constexpr auto frag_shader_file = "../glsl/frag.glsl";
constexpr auto compute_noise_file = "../glsl/compute.glsl";
constexpr auto compute_verts_file = "../glsl/compute_vertices.glsl";
constexpr auto simplex_noise_file = "../glsl/simplex_noise.glsl";

// ----------------------

static float delta_time = 0.f;
static float last_frame = 0.f;

// mouse position
static glm::vec2 mouse_last;
static struct Camera {
	glm::vec3 pos       = glm::vec3(0.f, 3.f, 3.f);
	glm::vec3 target    = glm::vec3(0.f, 3.f, 0.f);
	glm::vec3 dir       = glm::normalize(pos - target);

	const float speed = 0.05f;

	bool boost = false;	

	glm::vec3 right     = glm::normalize(glm::cross(glm::vec3(0,1,0), dir));
	glm::vec3 up        = glm::cross(dir, right);

	float yaw   = -90.f;
	float pitch = 0.f;
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
	glm::vec3 direction;
	direction.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
	direction.y = sin(glm::radians(camera.pitch));
	direction.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
	camera.target = glm::normalize(direction);
}

void key_callback(GLFWwindow *window, int key, int scancode, int act, int mod) {
	float speed = 20.f * delta_time * (camera.boost ? 4.f : 1.f);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.pos += speed * camera.target;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.pos -= speed * camera.target;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.pos -= 
			glm::normalize(glm::cross(camera.target, camera.up)) * speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    	camera.pos += 
    		glm::normalize(glm::cross(camera.target, camera.up)) * speed;
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera.boost = true;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
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

    Shader_program shader(vert_shader_file, frag_shader_file);
    Compute_program compute_noise({simplex_noise_file, compute_noise_file});
    Compute_program compute_verts({compute_verts_file});

    // ----------- noise generation ---------------
    GLuint noise_buffer;
    glGenBuffers(1, &noise_buffer);
    defer { glDeleteBuffers(1, &noise_buffer); };

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, noise_buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, WINDOW_W * sizeof(float) * WINDOW_H, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, noise_buffer);

    GLuint noise_texture;
    glGenTextures(1, &noise_texture);
    defer { glDeleteTextures(1, &noise_texture); };

    glBindTexture(GL_TEXTURE_2D, noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_W, WINDOW_H, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(0, noise_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    compute_noise.use();
    glDispatchCompute(WINDOW_W / 8, WINDOW_H / 8, 1);

    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    defer { glDeleteFramebuffers(1, &framebuffer); };

    // ---------- output framebuffer  ---------------
    compute_verts.use();
    GLuint ubo_config, ubo_camera;
    glGenBuffers(1, &ubo_config);
    defer { glDeleteBuffers(1, &ubo_config); };
    glGenBuffers(1, &ubo_camera);
    defer { glDeleteBuffers(1, &ubo_camera); };

    constexpr u32 group_size = 8;
    constexpr u32 chunk_size = 8;
    constexpr u32 total_size = group_size * chunk_size;

    // configuration
    struct Compute_config {
        glm::vec2 dims = glm::ivec2(total_size, total_size);
        glm::vec3 sky_color = glm::vec3(0.45, 0.716, 0.914);
        float clip_range = Z_FAR;
    } comput_conf;

    struct Compute_camera {
        glm::mat4 perspective;
        glm::mat4 view;

        glm::vec3 dir;
        glm::vec3 pos;
    } comput_cam;

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_config);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(comput_conf), &comput_conf, GL_STATIC_DRAW);
    compute_verts.ub_bind((GLchar*)"config", ubo_config);

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_camera);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(comput_cam), &comput_cam, GL_STATIC_DRAW);
    compute_verts.ub_bind((GLchar*)"config", ubo_camera);

	comput_cam.view = glm::lookAt(camera.pos, camera.pos + camera.target, camera.up);
	comput_cam.perspective = glm::perspective(glm::radians(FOV), ASPECT_RATIO, Z_NEAR, Z_FAR);

    glBindTexture(GL_TEXTURE_2D, noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_W, WINDOW_H, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(2, noise_texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

    GLuint output_texture;
    glGenTextures(1, &output_texture);
    defer { glDeleteTextures(1, &output_texture); };

    glBindTexture(GL_TEXTURE_2D, output_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_W, WINDOW_H, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(3, output_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERR("FRAMEBUFFER INCOMPLETE!");
        return EXIT_FAILURE;
    }    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDispatchCompute(WINDOW_W / 8, WINDOW_H / 8, 1);

	float near_plane = 1.0f, far_plane = 7.f;

    while (!glfwWindowShouldClose(window.get())) {
        float current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        glfwPollEvents();

        glDispatchCompute(WINDOW_W / 8, WINDOW_H / 8, 1);

        glViewport(0, 0, WINDOW_W, WINDOW_H);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, WINDOW_W, WINDOW_H, 0, 0, WINDOW_W, WINDOW_H, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Debug");
        ImGui::Text("camera_pos: {%.2f %.2f %.2f}", 
                camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("camera_tgt: {%.2f %.2f %.2f}", 
                camera.target.x, camera.target.y, camera.target.z);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
