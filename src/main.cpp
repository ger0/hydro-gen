#include <cstddef>
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <INIReader.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "rendering.hpp"
#include "utils.hpp"
#include "erosion.hpp"
#include "state.hpp"

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

struct Movement {
    enum class Side {
        NONE = 0,
        LEFT,
        RIGHT
    } side;
    enum class Front {
        NONE = 0,
        FORWARD,
        BACKWARD
    } front;
    bool boost = false;

    static constexpr float speed_cap = 100.0f;

    // update camera position
    void tick() {
	    float speed = 500.f * state.delta_frame * (state.camera.boost ? 4.0f : 1.f);
	    speed = speed > speed_cap ? speed_cap : speed;
    
	    if (front == Front::FORWARD)
		    state.camera.pos += speed * state.camera.dir;
	    else if (front == Front::BACKWARD)
		    state.camera.pos -= speed * state.camera.dir;
	    if (side == Side::LEFT)
		    state.camera.pos -= 
			    normalize(cross(state.camera.dir, state.camera.up)) * speed;
	    else if (side == Side::RIGHT)
    	    state.camera.pos += 
    		    normalize(cross(state.camera.dir, state.camera.up)) * speed;
    }
} movement;

void key_callback(GLFWwindow *window, 
        int key, int scancode, 
        int act, int mod) {

	auto& mv = movement;
	using mvf = Movement::Front;
	using mvs = Movement::Side;
    auto& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return;
    }
	constexpr auto& gkey = glfwGetKey;
	bool moving_left = false;
	bool moving_right = false;
	bool moving_up = false;
	bool moving_down = false;
    // L ALT
	if (gkey(window, GLFW_KEY_LEFT_ALT)) {
	    auto curs = glfwGetInputMode(window, GLFW_CURSOR);
	    if (curs == GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            mv.front = mvf::NONE;
            mv.side = mvs::NONE;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
        state.mouse_disabled = !state.mouse_disabled;
        return;
	}

	// W key
	if (gkey(window, GLFW_KEY_W)) {
	    moving_up = true;
	} else {
	    moving_up = false;
	}
    // S key
	if (gkey(window, GLFW_KEY_S)) {
	    moving_down = true;
	} else {
	    moving_down = false;
	}
    // A key
	if (gkey(window, GLFW_KEY_A)) {
	    moving_left = true;
	} else {
	    moving_left = false;
	}
    // D key
	if (gkey(window, GLFW_KEY_D)) {
	    moving_right = true;
	} else {
	    moving_right = false;
	}
    // spacebar
	if (gkey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		state.camera.boost = true;
	} if (gkey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
		state.camera.boost = false;
	}

	if (moving_left && !moving_right)       mv.side = mvs::LEFT;
	else if (moving_right && !moving_left)  mv.side = mvs::RIGHT;
	else                                    mv.side = mvs::NONE;

	if (moving_up && !moving_down)          mv.front = mvf::FORWARD;
	else if (moving_down && !moving_up)     mv.front = mvf::BACKWARD;
	else                                    mv.front = mvf::NONE;
}
};

int main(int argc, char* argv[]) { 
    srand(time(NULL));
    // load config...
    auto cwd = std::filesystem::current_path().string();
#ifdef __linux__
    cwd += "/config.ini";
#elif defined(_WIN32)
    cwd += "\\config.ini";
#endif
    auto write_to_ini = [](std::string path, const std::string& buffer) {
        FILE* file = fopen(path.c_str(), "wb");
        if (!file) {
            LOG_ERR("FAILED TO WRITE TO FILE: {}", path.c_str());
            exit(1);
        }
        defer { fclose(file); };
        const auto ret_val = fwrite(buffer.c_str(), buffer.length(), 1, file);
        if (ret_val != 1) {
		    LOG_ERR("Failed to write to config.ini!, path: {}", path.c_str());
		    exit(1);
        }
    };
    auto ini_config = INIReader(cwd);    
    if (ini_config.ParseError() < 0) {
        LOG("Failed to load config.ini, creating a new default config file...");
        const std::string config = "[window]\n"\
            "width = 1280\n"\
            "height = 720\n\n"\
            "[map]\n"\
            "size=1024\n\n"\
            "[erosion]\n"\
            "; type = grid or type = particle\n"\
            "type = grid\n"\
            "; particle_count works only when the erosion type is \"particle\"\n"\
            "particle_count = 262144";
        write_to_ini(cwd, config);
        ini_config = INIReader(cwd);    
    }
    const u32 WINDOW_W = ini_config.GetUnsigned("window", "width", 1280);
    const u32 WINDOW_H = ini_config.GetUnsigned("window", "height", 720);

    const u32 MAP_SIZE = ini_config.GetUnsigned("map", "size", 1024);

    Erosion::Programs::Erosion_type erosion_type;

    const std::string erosion_type_str = ini_config.Get("erosion", "type", "grid");
    const u32 particle_count = ini_config.GetUnsigned("erosion", "particle_count", 262144);
    
    if (erosion_type_str == "particle") {
        erosion_type = Erosion::Programs::PARTICLES;
    } else {
        erosion_type = Erosion::Programs::GRID;
    }

    // GLFW Window
    Uq_ptr<GLFWwindow, decltype(&destroy_window)> window(
        init_window(glm::uvec2{WINDOW_W, WINDOW_H}, "hydro-gen", &state.shader_error),
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
    auto settings = State::setup_settings(
        erosion_type == Erosion::Programs::PARTICLES,
        particle_count
    );
    defer{ State::delete_settings(settings); };

    // TODO: MOVE THIS OUT OF MAIN.CPP
    // Heightmap Generation Shader 
    Compute_program comput_map(noise_comput_file);
    // -------------

    // Ingame World Data (world state textures)
    State::World::Textures world_data = 
        State::World::gen_textures(MAP_SIZE);
    defer{delete_textures(world_data);};

    State::World::gen_heightmap(settings, world_data, comput_map);
    Uq_ptr<Erosion::Programs> erosion_progs_ptr(
        Erosion::setup_shaders(
            erosion_type, 
            settings, 
            world_data, 
            particle_count
        )
    );
    auto& erosion_progs = *erosion_progs_ptr.get();

    // ---------- prepare textures and framebuffer for rendering  ---------------
    auto renderer = Render::Data(
            WINDOW_W,
            WINDOW_H,
            MAP_SIZE,
            settings,
            state,
            world_data);

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
            Input_handling::movement.tick();
            state.last_frame = glfwGetTime();
        }

        // ---------- erosion compute shader ------------
        if (state.should_erode) {

            state.erosion_steps++;
            float erosion_d_time = glfwGetTime();
            if (erosion_type == Erosion::Programs::GRID) {
                if (state.should_rain) {
                    if (!(state.erosion_steps % settings.rain.data.period)) {
                        Erosion::dispatch_grid_rain(erosion_progs, world_data);
                    }
                }
                Erosion::dispatch_grid(erosion_progs, world_data);
            } 
            else if (erosion_type == Erosion::Programs::PARTICLES) {
                Erosion::dispatch_particle(erosion_progs, world_data, state.should_rain);
            }

            erosion_d_time = glfwGetTime() - erosion_d_time;
            state.erosion_time += erosion_d_time;
            // calculate average erosion update time
            if (!(state.erosion_steps % 100)) {
                state.erosion_mean_t = state.erosion_time / 100.f;
                state.erosion_time = 0;
            }
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
