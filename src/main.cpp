#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "input.hpp"
#include "rendering.hpp"
#include "utils.hpp"
#include "erosion.hpp"
#include "state.hpp"

constexpr u32 WINDOW_W = 800;
constexpr u32 WINDOW_H = 600;

constexpr auto noise_comput_file  = "heightmap.glsl";

int main(int argc, char* argv[]) { 
    srand(time(NULL));

    // GLFW Window
    Uq_ptr<GLFWwindow, decltype(&destroy_window)> window(
        init_window(glm::uvec2{WINDOW_W, WINDOW_H}, "Game", &State::global.shader_error),
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
    State::setup_settings();
    defer{ State::delete_settings(); };

    // TODO: MOVE THIS OUT OF MAIN.CPP
    // Heightmap Generation Shader 
    Compute_program comput_map(noise_comput_file);
    // -------------

    // Ingame World Data (world state textures)
    State::World::Textures world_data = State::World::gen_textures(NOISE_SIZE, NOISE_SIZE);
    defer{delete_textures(world_data);};

    State::World::gen_heightmap(comput_map);
    auto erosion_progs = Erosion::setup_shaders();

    // ---------- prepare textures and framebuffer for rendering  ---------------
    auto renderer = Render::Data(WINDOW_W, WINDOW_H, NOISE_SIZE, world_data);

    constexpr auto& state = State::global;
    while (!glfwWindowShouldClose(window.get()) && (!state.shader_error)) {
        glfwPollEvents();
        world_data.time = glfwGetTime();

        if (
            (world_data.time - state.last_frame + state.erosion_mean_t) >= (1 / state.target_fps) &&
            state.should_render
        ) {
            if (!renderer.dispatch(world_data)) {
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
                    Erosion::dispatch_grid_rain(erosion_progs, world_data, rain_settings);
                }
            }  */
            float erosion_d_time = glfwGetTime();
            // Erosion::dispatch_grid(erosion_progs, world_data);
            Erosion::dispatch_particle(erosion_progs, world_data);

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
            world_data, 
            comput_map
        );
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
