#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "rendering.hpp"
#include "utils.hpp"
#include "erosion.hpp"
#include "state.hpp"

constexpr u32 WINDOW_W = 1600;
constexpr u32 WINDOW_H = 900;

constexpr auto noise_comput_file  = "heightmap.glsl";

// mouse position
static glm::vec2 mouse_last;
State::Program_state state;

namespace Input_handling {

void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void key_callback(GLFWwindow *window, 
        int key, int scancode, 
        int act, int mod);

using glm::vec2, glm::vec3, glm::ivec2, glm::ivec3;
using glm::vec4, glm::ivec4;

void setup(GLFWwindow* window) {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);
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

	state.camera.yaw += xoffset;
	state.camera.pitch += yoffset;
	if (state.camera.pitch > 89.f) {
		state.camera.pitch = 89.f;
	}
	if (state.camera.pitch < -89.f) {
		state.camera.pitch = -89.f;
	}
	vec3 direction;
	direction.x = cos(glm::radians(state.camera.yaw)) 
	    * cos(glm::radians(state.camera.pitch));
	direction.y = sin(glm::radians(state.camera.pitch));
	direction.z = sin(glm::radians(state.camera.yaw)) 
	    * cos(glm::radians(state.camera.pitch));
	state.camera.dir = normalize(direction);
}

void key_callback(GLFWwindow *window, 
        int key, int scancode, 
        int act, int mod) {

    auto& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }
    constexpr float speed_cap = 100.0f;
	float speed = 200.f * state.delta_frame * (state.camera.boost ? 8.f : 1.f);
	speed = speed > speed_cap ? speed_cap : speed;
    
	constexpr auto& gkey = glfwGetKey;
	if (gkey(window, GLFW_KEY_W) == GLFW_PRESS)
		state.camera.pos += speed * state.camera.dir;
	if (gkey(window, GLFW_KEY_S) == GLFW_PRESS)
		state.camera.pos -= speed * state.camera.dir;
	if (gkey(window, GLFW_KEY_A) == GLFW_PRESS)
		state.camera.pos -= 
			normalize(cross(state.camera.dir, state.camera.up)) * speed;
	if (gkey(window, GLFW_KEY_D) == GLFW_PRESS)
    	state.camera.pos += 
    		normalize(cross(state.camera.dir, state.camera.up)) * speed;
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		state.camera.boost = true;
	}
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
		state.camera.boost = false;
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
};

int main(int argc, char* argv[]) { 
    srand(time(NULL));

    // GLFW Window
    Uq_ptr<GLFWwindow, decltype(&destroy_window)> window(
        init_window(glm::uvec2{WINDOW_W, WINDOW_H}, "Game", &state.shader_error),
        destroy_window
    );

    // Ingame input handling
    Input_handling::setup(window.get());
    assert(window.get() != nullptr);

    // Imgui init
    init_imgui(window.get());
    defer { destroy_imgui(); };

    // map gen + erosion settings from the UI
    // Sending uniform data to GPU
    auto settings = State::setup_settings();
    defer{ State::delete_settings(settings); };

    // TODO: MOVE THIS OUT OF MAIN.CPP
    // Heightmap Generation Shader 
    Compute_program comput_map(noise_comput_file);
    // -------------

    // Ingame World Data (world state textures)
    State::World::Textures world_data = 
        State::World::gen_textures(State::NOISE_SIZE, State::NOISE_SIZE);
    defer{delete_textures(world_data);};

    State::World::gen_heightmap(settings, world_data, comput_map);
    auto erosion_progs = Erosion::setup_shaders(settings, world_data);

    // ---------- prepare textures and framebuffer for rendering  ---------------
    auto renderer = Render::Data(
            WINDOW_W,
            WINDOW_H,
            State::NOISE_SIZE,
            settings,
            state,
            world_data);


    GLuint64 query_time;
    GLuint queryID;
    glGenQueries(1, &queryID);

    static float dong = 0.0;

    while (!glfwWindowShouldClose(window.get()) && (!state.shader_error)) {
        glfwPollEvents();
        world_data.time = glfwGetTime();

        if (
            (world_data.time - state.last_frame + state.erosion_mean_t) >= (1 / state.target_fps) &&
            state.should_render
        ) {
            if (!renderer.dispatch(world_data, settings, state.camera)) {
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

            // float erosion_d_time = glfwGetTime();
            glBeginQuery(GL_TIME_ELAPSED, queryID);

#if not defined(PARTICLE_COUNT) 
            if (state.should_rain) {
                if (!(state.erosion_steps % settings.rain.data.period)) {
                    Erosion::dispatch_grid_rain(erosion_progs, world_data);
                }
            }
            Erosion::dispatch_grid(erosion_progs, world_data);
#else
            Erosion::dispatch_particle(erosion_progs, world_data, state.should_rain);
#endif

            glEndQuery(GL_TIME_ELAPSED);
            glGetQueryObjectui64v(queryID, GL_QUERY_RESULT, &query_time);
            int done = 0;
            while (!done) {
                glGetQueryObjectiv(queryID, 
                        GL_QUERY_RESULT_AVAILABLE, 
                        &done);
            }

            // erosion_d_time = glfwGetTime() - erosion_d_time;
            state.erosion_time += query_time * 10e-9;
            dong += query_time * 10e-6;
            //state.erosion_time += erosion_d_time;
            // calculate average erosion update time
            if (!(state.erosion_steps % 100)) {
                state.erosion_mean_t = state.erosion_time / 100.f;
                state.erosion_time = 0;
            }

            /* if (state.erosion_steps == 20000) {
                dong /= state.erosion_steps;
                LOG_DBG("Total Erosion Time Average: {} {}", dong, state.erosion_steps);
                return EXIT_SUCCESS;
            } */
        }
        renderer.blit();
        renderer.handle_ui(
            settings,
            state,
            world_data, 
            comput_map
        );
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
