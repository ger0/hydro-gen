#include "state.hpp"

State::World::Textures State::World::gen_textures(const GLuint width, const GLuint height) {
    gl::Tex_pair heightmap(GL_READ_WRITE, width, height, BIND_HEIGHTMAP, BIND_WRITE_HEIGHTMAP);
    gl::Tex_pair flux(GL_READ_WRITE, width, height, BIND_FLUXMAP, BIND_WRITE_FLUXMAP);
    gl::Tex_pair velocity(GL_READ_WRITE, width, height, BIND_VELOCITYMAP, BIND_WRITE_VELOCITYMAP);
    gl::Tex_pair sediment(GL_READ_WRITE, width, height, BIND_SEDIMENTMAP, BIND_WRITE_SEDIMENTMAP);
    gl::Texture lockmap {
        .access = GL_READ_WRITE,
        .format = GL_R32UI,
    };
    gl::gen_texture(lockmap, GL_RED_INTEGER, GL_UNSIGNED_INT);
    gl::bind_texture(lockmap, BIND_LOCKMAP);

    // ------------- cross    flux for thermal erosion -----------
    gl::Tex_pair thermal_c(GL_READ_WRITE, width, height, BIND_THERMALFLUX_C, BIND_WRITE_THERMALFLUX_C);
    // ------------- diagonal flux for thermal erosion -----------
    gl::Tex_pair thermal_d(GL_READ_WRITE, width, height, BIND_THERMALFLUX_D, BIND_WRITE_THERMALFLUX_D);
    gl::Buffer particle_buffer {
        .binding = BIND_PARTICLE_BUFFER,
        .type = GL_SHADER_STORAGE_BUFFER
    };
    gl::gen_buffer(particle_buffer, PARTICLE_COUNT * sizeof(Particle));

    return State::World::Textures {
        .heightmap = heightmap,
        .flux = flux,
        .velocity = velocity,
        .sediment = sediment,
        .thermal_c = thermal_c,
        .thermal_d = thermal_d,
        .lockmap = lockmap,
        .particle_buffer = particle_buffer
    };
};

void State::World::delete_textures(State::World::Textures& data) {
    data.heightmap.delete_textures();
    data.velocity.delete_textures();
    data.flux.delete_textures();
    data.sediment.delete_textures();
    data.thermal_c.delete_textures();
    data.thermal_d.delete_textures();
    // gl::del_buffer(data.mass_buffer);
    gl::del_buffer(data.particle_buffer);
    gl::delete_texture(data.lockmap);
}

void State::setup_settings() {
    LOG_DBG("Generating settings buffers...");
    settings.rain.buffer.binding     = BIND_UNIFORM_RAIN_SETTINGS;
    settings.erosion.buffer.binding  = BIND_UNIFORM_EROSION;
    settings.map.buffer.binding      = BIND_UNIFORM_MAP_SETTINGS;

    gl::gen_buffer(settings.rain.buffer);
    gl::gen_buffer(settings.erosion.buffer);
    gl::gen_buffer(settings.map.buffer);

    settings.rain.push_data();
    settings.erosion.push_data();
    settings.map.push_data();
}

void State::delete_settings() {
    LOG_DBG("Deleting settings buffers...");
    gl::del_buffer(settings.rain.buffer);
    gl::del_buffer(settings.erosion.buffer);
    gl::del_buffer(settings.map.buffer);
}


void State::World::gen_heightmap(Compute_program& program) {
    program.use();
    State::settings.map.push_data();
    program.bind_uniform_block("map_settings", State::settings.map.buffer);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glUseProgram(0);
}
