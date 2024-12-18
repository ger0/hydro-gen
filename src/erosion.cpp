#include "erosion.hpp"
using namespace Erosion;

Programs Erosion::setup_shaders(State::Settings& set, State::World::Textures& data) {
    // compile compute shaders
    Programs prog = {
        .particle = {
            .movement   = Compute_program(particle_move_file),
            .erosion    = Compute_program(particle_erosion_file)
        },
        .grid = {
            .flux       = Compute_program(grid_hydro_flux_file),
            .erosion    = Compute_program(grid_hydro_erosion_file),
            .sediment   = Compute_program(grid_sediment_file),
            .rain       = Compute_program(grid_rain_comput_file)
        },
        .thermal = {
            .flux       = {
                Compute_program(thermal_flux_file),
                Compute_program(thermal_flux_file)
            },
            .transport  = {
                Compute_program(thermal_transport_file),
                Compute_program(thermal_transport_file)
            },
            .smooth     = Compute_program(smooth_file)
        },
    };

    for (int i = 0; i < SED_LAYERS; i++) {
        prog.thermal.flux[i].use();
        prog.thermal.flux[i].bind_uniform_block("erosion_data", set.erosion.buffer);
        prog.thermal.flux[i].set_uniform("t_layer", i);
        prog.thermal.transport[i].use();
        prog.thermal.transport[i].set_uniform("t_layer", i);
        glUseProgram(0);
    }

    prog.grid.flux.bind_uniform_block("erosion_data", set.erosion.buffer);
    prog.grid.erosion.bind_uniform_block("erosion_data", set.erosion.buffer);
    prog.grid.sediment.bind_uniform_block("erosion_data", set.erosion.buffer);

    prog.particle.movement.bind_uniform_block("map_settings", set.map.buffer);
    prog.particle.erosion.bind_uniform_block("erosion_data", set.erosion.buffer);

    prog.particle.movement.bind_storage_buffer("ParticleBuffer", data.particle_buffer);
    prog.particle.erosion.bind_storage_buffer("ParticleBuffer", data.particle_buffer);

    return prog;
}

void Erosion::dispatch_grid_rain(Programs& prog, const State::World::Textures& data) {
    prog.grid.rain.use();
    prog.grid.rain.set_uniform("time", data.time);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(
        State::NOISE_SIZE / WRKGRP_SIZE_X,
        State::NOISE_SIZE / WRKGRP_SIZE_Y, 
        1
    );
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

// helper functions
void run(Compute_program& program) {
    program.use();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(
        State::NOISE_SIZE / WRKGRP_SIZE_X,
        State::NOISE_SIZE / WRKGRP_SIZE_Y,
        1
    );
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
};

void run_thermal_erosion(Programs& prog, State::World::Textures& data) {
    for (int i = 0; i < SED_LAYERS; i++) {
        prog.thermal.flux[i].use();
        prog.thermal.flux[i].bind_texture("heightmap", data.heightmap.get_read_tex());
        prog.thermal.flux[i].bind_image("out_thflux_c", data.thermal_c.get_write_tex());
        prog.thermal.flux[i].bind_image("out_thflux_d", data.thermal_d.get_write_tex());
        run(prog.thermal.flux[i]);
        data.thermal_c.swap();
        data.thermal_d.swap();

        prog.thermal.transport[i].use();
        prog.thermal.transport[i].bind_texture("heightmap", data.heightmap.get_read_tex());
        prog.thermal.transport[i].bind_image("out_heightmap", data.heightmap.get_write_tex());
        prog.thermal.transport[i].bind_texture("thflux_c", data.thermal_c.get_read_tex());
        prog.thermal.transport[i].bind_texture("thflux_d", data.thermal_d.get_read_tex());
        run(prog.thermal.transport[i]);
        data.heightmap.swap();
    }
}

#if defined(PARTICLE_COUNT)
void run_particles(Compute_program& program) {
    program.use();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDispatchCompute(PARTICLE_COUNT / (WRKGRP_SIZE_X * WRKGRP_SIZE_Y), 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
};

void Erosion::dispatch_particle(Programs& prog, State::World::Textures& data, bool should_rain) {
    prog.particle.movement.use();
    prog.particle.movement.set_uniform("time", data.time);
    prog.particle.movement.set_uniform("should_rain", should_rain);
    prog.particle.movement.bind_texture("heightmap", data.heightmap.get_read_tex());
    prog.particle.movement.bind_texture("momentmap", data.velocity.get_read_tex());
    run_particles(prog.particle.movement);

    prog.particle.erosion.use();
    prog.particle.erosion.bind_image("lockmap", data.lockmap);
    prog.particle.erosion.bind_image("heightmap", data.heightmap.get_read_tex());
    prog.particle.erosion.bind_image("momentmap", data.velocity.get_read_tex());
    run_particles(prog.particle.erosion);

    run_thermal_erosion(prog, data);

    prog.thermal.smooth.use();
    prog.thermal.smooth.bind_image("heightmap", data.heightmap.get_read_tex());
    prog.thermal.smooth.bind_image("momentmap", data.velocity.get_read_tex());
    prog.thermal.smooth.bind_image("out_heightmap", data.heightmap.get_write_tex());
    prog.thermal.smooth.bind_image("out_momentmap", data.velocity.get_write_tex());
    run(prog.thermal.smooth);
    data.heightmap.swap(true);
    data.velocity.swap(true);
}
#endif

void Erosion::dispatch_grid(Programs& prog, State::World::Textures& data) {
    run(prog.grid.flux);
    data.heightmap.swap();
    data.flux.swap();
    data.velocity.swap();

    run(prog.grid.erosion);
    data.heightmap.swap();
    data.sediment.swap();

    run(prog.grid.sediment);
    data.heightmap.swap();
    data.sediment.swap();

    run_thermal_erosion(prog, data);

    run(prog.thermal.smooth);
    data.heightmap.swap();
}
