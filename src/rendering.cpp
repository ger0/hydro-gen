#include "rendering.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

constexpr auto render_comput_file = "rendering.glsl";

Render::Data::~Data() {
    glDeleteFramebuffers(1, &framebuffer);
    gl::delete_texture(output_texture);
    // LOG_DBG("Render data buffers deleted!");
}

Render::Data::Data(
        GLuint window_w,
        GLuint window_h,
        GLuint noise_size,
        State::Settings& set,
        State::Program_state& state,
        State::World::Textures& data
    ): 
        shader(Compute_program(render_comput_file)),
        output_texture({
            .target = GL_TEXTURE_2D, 
            .access = GL_WRITE_ONLY,
            .format = GL_RGBA8, 
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

    std::vector<float> test(1600*900*4);
    for (int i = 0; i < 15000; i++) {
        test[i] = 1.f;
    }
    gl::gen_texture(output_texture, GL_RGBA, GL_FLOAT, test.data());
    glGenFramebuffers(1, &framebuffer);
    // output image rendered to framebuffer
    shader.bind_uniform_block("map_settings", set.map.buffer);
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
        state.shader_error = true;
        return;
    }    

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CheckBoundImageUnits(GLuint maxUnits) {
    for (GLuint i = 0; i < maxUnits; ++i) {
        GLint boundTexture = 0;
        glGetIntegeri_v(GL_IMAGE_BINDING_NAME, i, &boundTexture);

        if (boundTexture != 0) {
            LOG_DBG("Image unit {} has texture bound with ID: {}", i, boundTexture);
        } else {
            LOG_DBG("Image unit {} is unbound.", i);
        }
    }
}

bool Render::Data::dispatch(
    State::World::Textures& data,
    State::Settings& set,
    State::Program_state::Camera& cam
) {
    using glm::perspective, glm::lookAt, glm::radians;
    shader.use();

    // TODO: change to a uniform buffer
    glm::mat4 mat_v = lookAt(
	    cam.pos, 
	    cam.pos + cam.dir, 
	    cam.up
	);
    const float aspect_ratio = float(window_dims.w) / window_dims.h;
    glm::mat4 mat_p = perspective(
	    radians(FOV), 
	    aspect_ratio, 
	    Z_NEAR, Z_FAR
	);

    // CheckBoundImageUnits(8);
    shader.use();
    shader.bind_image("out_tex", output_texture);
    shader.bind_texture("sedimentmap", data.sediment.get_read_tex());
    shader.bind_texture("heightmap", data.heightmap.get_read_tex());

    shader.set_uniform("view", mat_v);
    shader.set_uniform("perspective", mat_p);
    shader.set_uniform("pos", cam.pos);
    shader.set_uniform("prec", prec);

#if not defined(PARTICLE_COUNT)
    shader.set_uniform("display_sediment", display_sediment);
#endif
    //shader.set_uniform("sediment_max_cap", set.erosion.data.Kc);
    shader.set_uniform("DEBUG_PREVIEW", debug_preview);
    shader.set_uniform("should_draw_water", display_water);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

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

    shader.unbind_image("out_tex");
    shader.unbind_texture("sedimentmap");
    shader.unbind_texture("heightmap");
    return true;
}

void heightmap_ui(
    Render::Data *rendr,
    State::Settings& set,
    State::Program_state& state,
    State::World::Textures& world,
    Compute_program& map_generator
) {
    auto& rain    = set.rain;
    auto& map     = set.map;
    auto& erosion = set.erosion;

    ImGui::Checkbox("Heightmap view", &rendr->debug_preview);
    ImGui::Checkbox("Display water", &rendr->display_water);
    ImGui::SliderFloat("Raymarching precision", &rendr->prec, 0.01f, 1.f);
    ImGui::SliderFloat("Target_fps", &state.target_fps, 2.f, 120.f);
    ImGui::SliderFloat("Time step", &erosion.data.d_t, 0.0005f, 0.05f);
    ImGui::End();

    ImGui::Begin("Heightmap");
    ImGui::SliderFloat("Seed", &map.data.seed, 0.0f, 1e4);
    ImGui::SliderFloat("Height multiplier", &map.data.height_mult, 0.1f, 2.f);

    ImGui::SliderFloat("Dirt max height", &map.data.max_dirt, 0.0f, 10.f);

    ImGui::SliderFloat("Persistence", &map.data.persistance, 0.0f, 1.f);
    ImGui::SliderFloat("Lacunarity", &map.data.lacunarity, 0.0f, 4.f);
    ImGui::SliderFloat("Scale", &map.data.scale, 0.0f, 0.01f, "%.5f");
    // ImGui::SliderFloat("Redistribution", &map.data.redistribution, 0.0f, 100.f);
    ImGui::SliderInt("Octaves", &map.data.octaves, 1, 10);

    ImGui::SeparatorText("Domain warping");
    ImGui::SliderInt("Layers", &map.data.domain_warp, 0, 2);
    ImGui::SliderFloat("Domain Scale", &map.data.domain_warp_scale, 20.f, 1000.f);

    ImGui::SeparatorText("Options");
    //ImGui::Checkbox("Fake erosion", (bool*)&map.data.fake_erosion);
    ImGui::Checkbox("Mask Circular", (bool*)&map.data.mask_round);
    ImGui::Checkbox("Mask Exp", (bool*)&map.data.mask_exp);
    ImGui::Checkbox("Mask Power3", (bool*)&map.data.mask_power);
    ImGui::Checkbox("Mask Slope", (bool*)&map.data.mask_slope);
    ImGui::Checkbox("Uplift", (bool*)&map.data.uplift);

    ImGui::SliderFloat("Uplift scale", &map.data.uplift_scale, 0.5f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic );

    ImGui::SeparatorText("Terracing");
    ImGui::SliderInt("Terrace levels", &map.data.terrace, 0, 30);
    ImGui::SliderFloat("Terrace scale", &map.data.terrace_scale, 0.f, 1.f);
    
    if (ImGui::Button("Generate")) {
        bool old_erod = state.should_erode;
        state.should_erode = false;
        delete_textures(world);
        world = State::World::gen_textures(State::NOISE_SIZE, State::NOISE_SIZE);
        State::World::gen_heightmap(set, world, map_generator);
        erosion.push_data();
        state.should_erode = old_erod;
    }
}

void erosion_ui(State::Settings& set) {
    auto& erosion = set.erosion;

    ImGui::SliderFloat("Capacity", &erosion.data.Kc, 0.001f, 1.00f, "%.3f");
    ImGui::SliderFloat("Evaporation", &erosion.data.Ke, 0.0f, 1.00f, "%.4f");
    ImGui::SliderFloat("Gravitation", &erosion.data.G, 0.1f, 10.f);
    ImGui::SliderFloat("Conversion rate", &erosion.data.Kconv, 1, 1e-4, "%.4f");
    if (ImGui::BeginTabBar("Erosion Settings")) {
        for (u32 i = 0; i < 2; i++) {
            auto name = i == 0 ? "Rock Layer" : "Dirt Layer";
            if (ImGui::BeginTabItem(name)) {
                ImGui::SliderFloat("Solubility", &erosion.data.Ks[i], 0.0001f, 0.50f, "%.4f");
                ImGui::SliderFloat("Deposition", &erosion.data.Kd[i], 0.0001f, 0.50f, "%.4f");
                ImGui::SliderAngle("Talus angle", &erosion.data.Kalpha[i], 0.00, 90.f);
                // TODO: move this somewhere else...
                #if defined(PARTICLE_COUNT) 
                    ImGui::SliderFloat("Slippage speed", &erosion.data.Kspeed[i], 0.0001, 0.19f, "%.5f", ImGuiSliderFlags_Logarithmic);
                #else 
                    ImGui::SliderFloat("Slippage speed", &erosion.data.Kspeed[i], 0.01, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic);
                #endif
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void rain_particle_ui(State::Settings& set, State::Program_state& state) {
    auto& rain    = set.rain;
#if defined(PARTICLE_COUNT)
    auto& erosion = set.erosion;
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
    if (ImGui::Button(state.should_rain ? "Stop Raining" : "Rain")) {
        state.should_rain = !state.should_rain;
    }
#else
    ImGui::SeparatorText("Rain");
    if (ImGui::Button(state.should_rain ? "Stop Raining" : "Rain")) {
        state.should_rain = !state.should_rain;
        rain.push_data();
    }
    ImGui::SliderFloat("Amount", &rain.data.amount, 0.0f, 1.0f, "%.5f");
    ImGui::SliderFloat("Bonus (%)", &rain.data.mountain_thresh, 0.0f, 1.0f);
    ImGui::SliderFloat("Bonus Amount", &rain.data.mountain_multip, 0.0f, 2.5f, "%.5f");
    ImGui::SliderInt("Tick period", &rain.data.period, 2, 10000);
    ImGui::SliderFloat("Drops", &rain.data.drops, 0.001, 0.1);
#endif 
}

// TODO: Refactor
void Render::Data::handle_ui(
    State::Settings& set,
    State::Program_state& state,
    State::World::Textures& world,
    Compute_program& map_generator
) {
    auto& erosion = set.erosion;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
    ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::Text("Camera pos: {%.2f %.2f %.2f}", 
        state.camera.pos.x, 
        state.camera.pos.y, 
        state.camera.pos.z
    );
    ImGui::Text("Camera dir: {%.2f %.2f %.2f}", 
        state.camera.dir.x, 
        state.camera.dir.y, 
        state.camera.dir.z
    );
    ImGui::Text("Frame time (ms): {%.2f}", state.frame_t);
    ImGui::Text("FPS: {%.2f}", 1000.0 / state.frame_t);
    ImGui::Text("Total erosion updates: {%lu}", state.erosion_steps);
    // ImGui::Text("Avg erosion time: {%f}", state.erosion_mean_t);
    ImGui::Text("Total Time: {%f}", world.time);
    ImGui::End();

    ImGui::Begin("Settings");
    if (ImGui::Button(state.should_render ? "Disable Rendering" : "Start Rendering")) {
        state.should_render = !state.should_render;
    }

    ImGui::SeparatorText("Erosion Settings");
    erosion_ui(set);
    if (ImGui::Button("Set Erosion settings")) {
        erosion.push_data();
    }
    ImGui::SameLine();
    if (ImGui::Button(state.should_erode ? "Stop Erosion" : "Erode")) {
        state.should_erode = !state.should_erode;
    }

    rain_particle_ui(set, state);

    ImGui::SeparatorText("General");
#if not defined(PARTICLE_COUNT)
    ImGui::SliderFloat("Energy Kept (%)", &erosion.data.ENERGY_KEPT, 0.998, 1.0, "%.5f");
    ImGui::Checkbox("Display sediment", &display_sediment);
#endif

heightmap_ui(
    this,
    set,
    state,
    world,
    map_generator
);

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
