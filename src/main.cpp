#include <glm/glm.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shaderprogram.hpp"
#include "utils.hpp"

constexpr u32 WINDOW_W = 1280;
constexpr u32 WINDOW_H = 720;

constexpr auto vert_shader_file = "../glsl/vert.glsl";
constexpr auto frag_shader_file = "../glsl/frag.glsl";
constexpr auto compute_noise_file = "../glsl/compute.glsl";
constexpr auto compute_verts_file = "../glsl/compute_vertices.glsl";
constexpr auto simplex_noise_file = "../glsl/simplex_noise.glsl";

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
    defer { destroy_imgui(); };

    Shader_program shader(vert_shader_file, frag_shader_file);
    Compute_program compute_noise({simplex_noise_file, compute_noise_file});
    Compute_program compute_verts({compute_verts_file});

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

    GLuint ubo_compute_config;
    glGenBuffers(1, &ubo_compute_config);
    defer { glDeleteBuffers(1, &ubo_compute_config); };

    // configuration
    struct Compute_verts_config {
        glm::vec2 dims = glm::ivec2(WINDOW_W, WINDOW_H);
        glm::uint vert_buff_size = 4;
    } verts_config;

    glBindBuffer(GL_UNIFORM_BUFFER, ubo_compute_config);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(verts_config), &verts_config, GL_STATIC_DRAW);
    compute_verts.ub_bind((GLchar*)"config", ubo_compute_config);

    // glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(verts_config), &verts_config);

    GLuint buff_compute_verts;
    glGenBuffers(1, &buff_compute_verts);
    defer { glDeleteBuffers(1, &buff_compute_verts); };

    struct Vert {
        glm::vec4 position;
        glm::vec4 normal;
    };

    // TODO: only for testing
    constexpr u32 tst = 4;
    Vert vert_arr[tst] = {};

    compute_verts.use();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buff_compute_verts);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Vert) * tst, vert_arr, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buff_compute_verts);

    glBindTexture(GL_TEXTURE_2D, noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_W, WINDOW_H, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(2, noise_texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

    glDispatchCompute(WINDOW_W / 8, WINDOW_H / 8, 1);

    float delta_time = 0.f;
    float last_frame = 0.f;

    shader.use();

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
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, WINDOW_W, WINDOW_H, 0, 0, WINDOW_W, WINDOW_H, GL_COLOR_BUFFER_BIT, GL_NEAREST);

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
