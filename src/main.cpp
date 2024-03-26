#include <glm/glm.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "utils.hpp"

constexpr u32 WINDOW_W = 800;
constexpr u32 WINDOW_H = 600;

constexpr auto vert_shader = "../glsl/vert.glsl";
constexpr auto frag_shader = "../glsl/frag.glsl";

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

    assert(window.get() != nullptr);

    init_imgui(window.get());

    Uq_ptr<Shader_program, decltype(&destroy_shader)> shader(
            create_shader(vert_shader, frag_shader),
            destroy_shader
        );

    shader->use();

    float delta_time = 0.f;
    float last_frame = 0.f;

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

        // imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("TEST");
        ImGui::Text("Hello");

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window.get());
    }
    LOG_DBG("Closing the program...");
    return 0;
}
