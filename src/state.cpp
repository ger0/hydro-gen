#include "state.hpp"

State::World::Textures State::World::gen_textures(
    const GLuint size,
    const GLuint particle_count
) {
    gl::Texture lockmap {
        .access = GL_READ_WRITE,
        .format = GL_R32UI,
        .width = size,
        .height = size
    };
    gl::gen_texture(lockmap, GL_RED_INTEGER, GL_UNSIGNED_INT);
    gl::Tex_pair heightmap(GL_READ_WRITE, size, size);
    gl::Tex_pair flux(GL_READ_WRITE, size, size);
    gl::Tex_pair velocity(GL_READ_WRITE, size, size);
    gl::Tex_pair sediment(GL_READ_WRITE, size, size);

    // ------------- cross    flux for thermal erosion -----------
    gl::Tex_pair thermal_c(GL_READ_WRITE, size, size);
    // ------------- diagonal flux for thermal erosion -----------
    gl::Tex_pair thermal_d(GL_READ_WRITE, size, size);

    gl::Buffer particle_buffer {
        .binding = BIND_PARTICLE_BUFFER,
        .type = GL_SHADER_STORAGE_BUFFER
    };
    if (particle_count) {
        gl::gen_buffer(particle_buffer, particle_count * sizeof(Particle));
    }

    return State::World::Textures {
        .map_size = size,
        .particle_count = particle_count,
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
    gl::del_buffer(data.particle_buffer);
    gl::delete_texture(data.lockmap);
}

State::Settings State::setup_settings(bool is_particle, u32 particle_count) {
    LOG_DBG("Generating settings buffers...");
    Settings set;
    if (is_particle) {
        set.erosion.data = {
            .particle_count = GLuint(particle_count),
            .Kc             = 0.2000,
            .Kalpha         = VEC2(1.3f, 0.6f),
            .Kconv          = 0.001,
            .Ks             = VEC2(0.03, 0.09),
            .Kd             = VEC2(0.01, 0.03),
            .Ke             = 0.03,
            .Kspeed         = VEC2(0.002f, 0.008f),
            .G              = 9.81,
            .d_t            = 0.25,
            .density        = 1.0,
            .init_volume    = 1.0,
            .friction       = 0.20,
            .inertia        = 1.00,
            .min_volume     = 0.00,
            .min_velocity   = 0.001,
            .ttl            = 15000,
        };
    } else {
        set.erosion.data = {
            .Kc             = 0.2000,
            .Kalpha         = VEC2(1.3f, 0.6f),
            .Kconv          = 0.001,
            .Ks             = VEC2(0.03, 0.09),
            .Kd             = VEC2(0.01, 0.03),
            .Ke             = 0.03,
            .ENERGY_KEPT    = 1.0,
            .Kspeed         = VEC2(0.5f, 2.0f),
            .G              = 1.0,
            .d_t            = 0.001,
        };
    }
    set.rain.buffer.binding     = BIND_UNIFORM_RAIN_SETTINGS;
    set.erosion.buffer.binding  = BIND_UNIFORM_EROSION;
    set.map.buffer.binding      = BIND_UNIFORM_MAP_SETTINGS;

    gl::gen_buffer(set.rain.buffer);
    gl::gen_buffer(set.erosion.buffer);
    gl::gen_buffer(set.map.buffer);

    set.rain.push_data();
    set.erosion.push_data();
    set.map.push_data();
    return set;
}

void State::delete_settings(State::Settings& set) {
    LOG_DBG("Deleting settings buffers...");
    gl::del_buffer(set.rain.buffer);
    gl::del_buffer(set.erosion.buffer);
    gl::del_buffer(set.map.buffer);
}


void State::World::gen_heightmap(
    Settings& settings,
    State::World::Textures& world,
    Compute_program& program
) {
    program.use();

    settings.map.push_data();
    program.bind_uniform_block("map_settings", settings.map.buffer);
    program.bind_storage_buffer("ParticleBuffer", world.particle_buffer);

    program.bind_image("dest_heightmap", world.heightmap.get_write_tex());
    program.bind_image("dest_vel", world.velocity.get_write_tex());
    program.bind_image("dest_flux", world.flux.get_write_tex());
    program.bind_image("dest_sediment", world.sediment.get_write_tex());

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glDispatchCompute(world.map_size / WRKGRP_SIZE_X, world.map_size / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    world.heightmap.swap(true);
    world.velocity.swap(true);
    world.flux.swap(true);
    world.sediment.swap(true);

    program.unbind_image("dest_heightmap");
    program.unbind_image("dest_vel");
    program.unbind_image("dest_flux");
    program.unbind_image("dest_sediment");

    glUseProgram(0);
}
