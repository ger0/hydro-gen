#include "rendering.hpp"
#include "input.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

constexpr auto render_comput_file = "rendering.glsl";

Render::Data::~Data() {
    glDeleteFramebuffers(1, &framebuffer);
    gl::delete_texture(output_texture);
    LOG_DBG("Render data buffers deleted!");
}

Render::Data::Data(
        GLuint window_w,
        GLuint window_h,
        GLuint noise_size,
        State::World::Textures& data
    ): 
        shader(Compute_program(render_comput_file)),
        output_texture({
            .target = GL_TEXTURE_2D, 
            .access = GL_WRITE_ONLY,
            .format = GL_RGBA32F, 
            .width  = window_w,
            .height = window_h
        }),
        window_dims({
            .w = (GLuint)window_w,
            .h = (GLuint)window_h
        }),
        aspect_ratio((float)window_w / window_h) {
    shader.use();
    LOG_DBG("Setting up rendering shader!");

    gl::gen_texture(output_texture);
    gl::bind_texture(output_texture, BIND_DISPLAY_TEXTURE);
    glGenFramebuffers(1, &framebuffer);
    // output image rendered to framebuffer
    shader.bind_uniform_block("map_settings", State::settings.map.buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, 
        GL_TEXTURE_2D, 
        output_texture.texture, 
        0
    );

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
            != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERR("Rendering::Framebuffer Incomplete!");
        State::global.shader_error = true;
        return;
    }    

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Render::Data::dispatch(State::World::Textures& data) {
    using glm::perspective, glm::lookAt, glm::radians;
    shader.use();

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // TODO: change to a uniform buffer
    glm::mat4 mat_v = lookAt(
	    Input_handling::camera.pos, 
	    Input_handling::camera.pos + Input_handling::camera.dir, 
	    Input_handling::camera.up
	);
    const float aspect_ratio = float(window_dims.w) / window_dims.h;
    glm::mat4 mat_p = perspective(
	    radians(FOV), 
	    aspect_ratio, 
	    Z_NEAR, Z_FAR
	);

    shader.set_uniform("view", mat_v);
    shader.set_uniform("perspective", mat_p);
    shader.set_uniform("dir", Input_handling::camera.dir);
    shader.set_uniform("pos", Input_handling::camera.pos);
    shader.set_uniform("time", data.time);
    shader.set_uniform("prec", prec);
    shader.set_uniform("display_sediment", display_sediment);
    shader.set_uniform("sediment_max_cap", State::settings.erosion.data.Kc);
    shader.set_uniform("DEBUG_PREVIEW", debug_preview);

#ifdef LOW_RES_DIV3
    glDispatchCompute(
        window_dims.w / (3 * WRKGRP_SIZE_X),
        window_dims.h / (3 * WRKGRP_SIZE_Y),
        1
    );
#else
    glDispatchCompute(
        window_dims.w / (WRKGRP_SIZE_X),
        window_dims.h / (WRKGRP_SIZE_Y),
        1
    );
#endif
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    return true;
}

// TODO: Refactor
void Render::Data::handle_ui(
    State::World::Textures& world,
    Compute_program& map_generator
) {
    // imgui
    constexpr auto& erosion = State::settings.erosion;
    constexpr auto& rain    = State::settings.rain;
    constexpr auto& map     = State::settings.map;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::Text("Camera pos: {%.2f %.2f %.2f}", 
        Input_handling::camera.pos.x, 
        Input_handling::camera.pos.y, 
        Input_handling::camera.pos.z
    );
    ImGui::Text("Input_handling::camera dir: {%.2f %.2f %.2f}", 
        Input_handling::camera.dir.x, 
        Input_handling::camera.dir.y, 
        Input_handling::camera.dir.z
    );
    ImGui::Text("Frame time (ms): {%.2f}", State::global.frame_t);
    ImGui::Text("FPS: {%.2f}", 1000.0 / State::global.frame_t);
    ImGui::Text("Total erosion updates: {%lu}", State::global.erosion_steps);
    ImGui::Text("Avg erosion time: {%f}", State::global.erosion_mean_t);
    ImGui::Text("Total Time: {%f}", world.time);
    ImGui::End();

    ImGui::Begin("Settings");
    if (ImGui::Button(State::global.should_render ? "Disable Rendering" : "Start Rendering")) {
        State::global.should_render = !State::global.should_render;
    }
    ImGui::SameLine();
    if (ImGui::Button(State::global.should_erode ? "Stop Erosion" : "Erode")) {
        State::global.should_erode = !State::global.should_erode;
    }
    ImGui::SameLine();
    if (ImGui::Button(State::global.should_rain ? "Stop Raining" : "Rain")) {
        State::global.should_rain = !State::global.should_rain;
        rain.push_data();
    }

#if defined(PARTICLE_COUNT)
    ImGui::SeparatorText("Particles");
    ImGui::SliderFloat("Density", &erosion.data.density, 0.0f, 2.0f, "%.1f");
    ImGui::SliderFloat("Initial volume", &erosion.data.init_volume, 0.0f, 2.0f, "%.1f");
    ImGui::SliderFloat("Friction", &erosion.data.friction, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Inertia", &erosion.data.inertia, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Minimum volume", &erosion.data.min_volume, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Minimum velocity", &erosion.data.min_velocity, 0.0f, 1.0f, "%.3f");
    int ttl = (int)erosion.data.ttl;
    ImGui::SliderInt("Maximum iterations", &ttl, 1, 5000);
    erosion.data.ttl = (GLuint)ttl;
#else
    ImGui::SeparatorText("Rain");
    ImGui::SliderFloat("Amount", &rain.data.amount, 0.0f, 1.0f, "%.5f");
    ImGui::SliderFloat("Bonus (%)", &rain.data.mountain_thresh, 0.0f, 1.0f);
    ImGui::SliderFloat("Bonus Amount", &rain.data.mountain_multip, 0.0f, 2.5f, "%.5f");
    ImGui::SliderInt("Tick period", &rain.data.period, 2, 10000);
    ImGui::SliderFloat("Drops", &rain.data.drops, 0.001, 0.1);
#endif 
    ImGui::SeparatorText("Hydraulic Erosion");
    ImGui::SliderFloat("Capacity", &erosion.data.Kc, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Solubility", &erosion.data.Ks, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Deposition", &erosion.data.Kd, 0.0001f, 0.10f, "%.4f");
    ImGui::SliderFloat("Evaporation", &erosion.data.Ke, 0.0f, 1.00f);
    ImGui::SliderFloat("Gravitation", &erosion.data.G, 0.1f, 10.f);

    ImGui::SeparatorText("Thermal Erosion");
    ImGui::SliderAngle("Talus angle", &erosion.data.Kalpha, 0.00, 90.f);

    // TODO: move this somewhere else...
    #if defined(PARTICLE_COUNT) 
        ImGui::SliderFloat("Erosion speed", &erosion.data.Kspeed, 0.0001, 1.f, "%.5f", ImGuiSliderFlags_Logarithmic);
    #else 
        ImGui::SliderFloat("Erosion speed", &erosion.data.Kspeed, 0.01, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Energy Kept (%)", &erosion.data.ENERGY_KEPT, 0.998, 1.0, "%.5f");
    #endif
    if (ImGui::Button("Set Erosion Settings")) {
        erosion.push_data();
    }
    ImGui::SeparatorText("General");
    ImGui::SliderFloat("Raymarching precision", &prec, 0.01f, 1.f);
    ImGui::Checkbox("Display sediment", &display_sediment);
    ImGui::Checkbox("Heightmap view", &debug_preview);
    ImGui::SliderFloat("Target_fps", &State::global.target_fps, 2.f, 120.f);
    ImGui::SliderFloat("Time step", &erosion.data.d_t, 0.0005f, 0.05f);
    ImGui::End();

    ImGui::Begin("Heightmap");
    ImGui::SliderFloat("Seed", &map.data.seed, 0.0f, 1e4);
    ImGui::SliderFloat("Height multiplier", &map.data.height_mult, 0.1f, 2.f);
    ImGui::SliderFloat("Persistence", &map.data.persistance, 0.0f, 1.f);
    ImGui::SliderFloat("Lacunarity", &map.data.lacunarity, 0.0f, 4.f);
    ImGui::SliderFloat("Scale", &map.data.scale, 0.0f, 0.01f, "%.5f");
    // ImGui::SliderFloat("Redistribution", &map.data.redistribution, 0.0f, 100.f);
    ImGui::SliderInt("Octaves", &map.data.octaves, 1, 10);

    ImGui::SeparatorText("Masking");
    ImGui::Checkbox("Circular", (bool*)&map.data.mask_round);
    ImGui::Checkbox("Exp", (bool*)&map.data.mask_exp);
    ImGui::Checkbox("Power3", (bool*)&map.data.mask_power);
    ImGui::Checkbox("Slope", (bool*)&map.data.mask_slope);
    
    if (ImGui::Button("Generate")) {
        bool old_erod = State::global.should_erode;
        State::global.should_erode = false;
        delete_textures(world);
        world = State::World::gen_textures(NOISE_SIZE, NOISE_SIZE);
        State::World::gen_heightmap(map_generator);
        erosion.push_data();
        State::global.should_erode = old_erod;
    }
    ImGui::End();
    ImGui::Render();
}

void Render::Data::blit() {
    glBlitNamedFramebuffer(
        framebuffer, 0, 
        0, 0, window_dims.w, window_dims.h, 
        0, 0, window_dims.w, window_dims.h, 
        GL_COLOR_BUFFER_BIT, GL_NEAREST
    );
}
