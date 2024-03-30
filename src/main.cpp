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

float delta_time = 0.f;
float last_frame = 0.f;

// mouse position
glm::vec2 mouse_last;
struct Camera {
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

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glFrontFace(GL_CW);
	glEnable(GL_BLEND);  

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, noise_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        return EXIT_FAILURE;
    }    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------- vertex generation ---------------
    GLuint ubo_compute_config;
    glGenBuffers(1, &ubo_compute_config);
    defer { glDeleteBuffers(1, &ubo_compute_config); };

    constexpr u32 group_size = 8;
    constexpr u32 chunk_size = 8;
    constexpr u32 total_size = group_size * chunk_size;

    // configuration
    struct Compute_verts_config {
        glm::vec2 dims = glm::ivec2(total_size, total_size);
        glm::uint vert_buff_size = total_size;
    } verts_config;

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_compute_config);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(verts_config), &verts_config, GL_STATIC_DRAW);
    compute_verts.ub_bind((GLchar*)"config", ubo_compute_config);

    GLuint buff_compute_verts;
    glGenBuffers(1, &buff_compute_verts);
    defer { glDeleteBuffers(1, &buff_compute_verts); };

    struct Vert {
        glm::vec3 position;
        glm::vec3 normal;
    };

    compute_verts.use();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buff_compute_verts);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Vert) * total_size, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buff_compute_verts);

    glBindTexture(GL_TEXTURE_2D, noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_W, WINDOW_H, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(2, noise_texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

    glDispatchCompute(chunk_size, chunk_size, 1);

    // ---------- rendering ----------
    shader.use();

    constexpr glm::vec3 SKY_COLOR(0.45, 0.716, 0.914);
    constexpr auto  SUN_DIRECTION = glm::vec3(0, 1, -0.12);
    constexpr float FOG_DENSITY   = 0.0036;
    constexpr float FOG_GRADIENT  = 8.0;

	glClearColor(SKY_COLOR.r, SKY_COLOR.g, SKY_COLOR.b, 1.f);
	glUniform3fvARB(shader.u("skyColor"), 1, glm::value_ptr(SKY_COLOR));
	glUniform1f(shader.u("far"), Z_FAR);
	glUniform1f(shader.u("near"), Z_NEAR);
	glUniform3fv(shader.u("sunDir"), 1, glm::value_ptr(
				glm::normalize(SUN_DIRECTION)));
	glUniform1f(shader.u("density"),  FOG_DENSITY);
	glUniform1f(shader.u("gradient"), FOG_GRADIENT);

	float near_plane = 1.0f, far_plane = 7.f;
	static glm::mat4 light_proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
	static glm::mat4 light_view = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f), 
                                  	  glm::vec3( 0.0f, 0.0f,  0.0f), 
                                  	  glm::vec3( 0.0f, 1.0f,  0.0f));
	static glm::mat4 light_space_mat = light_proj * light_view; 
	glUniformMatrix4fv(shader.u("lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(light_space_mat));

    GLuint vao;
    glGenVertexArrays(1, &vao);

    auto M = glm::mat4(1.f);
	auto V = glm::lookAt(camera.pos, camera.pos + camera.target, camera.up);
	auto P = glm::perspective(glm::radians(FOV), ASPECT_RATIO, Z_NEAR, Z_FAR);

	glUniformMatrix4fv(shader.u("M"), 1, false, glm::value_ptr(M));
	glUniform3fvARB(shader.u("camera_pos"), 1, glm::value_ptr(camera.pos));
	glUniformMatrix4fv(shader.u("V"), 1, false, glm::value_ptr(V));
	glUniformMatrix4fv(shader.u("P"), 1, false, glm::value_ptr(P));

	glBindVertexArray(vao);
	// binding vertices generated earlier
	glBindBuffer(GL_ARRAY_BUFFER, buff_compute_verts);

	uint count = 3;
	// passing position vector to vao
	glVertexAttribPointer(shader.a("vertex"), count, GL_FLOAT, GL_FALSE,
        	sizeof(Vert), (GLvoid*)offsetof(Vert, position));
	glEnableVertexAttribArray(shader.a("vertex"));

	/* // passing normal vector to vao
	glVertexAttribPointer(shader.a("normal"), count, GL_FLOAT, GL_FALSE,
        	sizeof(Vert), (GLvoid*)offsetof(Vert, normal));
	glEnableVertexAttribArray(shader.a("normal")); */

	/* glVertexAttribPointer(sp->a("color"), count + 1, GL_FLOAT, GL_FALSE,
        	sizeof(Vertex), (GLvoid*)offsetof(Vert, color));
	glEnableVertexAttribArray(sp->a("color")); */

	glDrawArrays(GL_TRIANGLES, 0, total_size);

	// glBindVertexArray(0);

    while (!glfwWindowShouldClose(window.get())) {
        float current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        glfwPollEvents();
        // 1 rendering pass
        /* {
           glViewport(0, 0, SHADOW_W, SHADOW_H);
           configureShaderAndMatrices(spshad.get());
           wglBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
           glClear(GL_DEPTH_BUFFER_BIT);
           glBindFramebuffer(GL_FRAMEBUFFER, 0);
        } */

        // 2 rendering pass
        glViewport(0, 0, WINDOW_W, WINDOW_H);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	    glDrawArrays(GL_TRIANGLES, 0, total_size);
        /* glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, WINDOW_W, WINDOW_H, 0, 0, WINDOW_W, WINDOW_H, GL_COLOR_BUFFER_BIT, GL_NEAREST); */

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
