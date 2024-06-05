#include "erosion.hpp"
using namespace Erosion;

Programs Erosion::setup_shaders() {
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
            .flux       = Compute_program(thermal_flux_file),
            .transport  = Compute_program(thermal_transport_file),
            .smooth     = Compute_program(smooth_file)
        },
    };

    constexpr auto& set = State::settings;

    prog.thermal.flux.bind_uniform_block("Erosion_data", set.erosion.buffer);
    prog.thermal.transport.bind_uniform_block("Erosion_data", set.erosion.buffer);

    prog.grid.flux.bind_uniform_block("Erosion_data", set.erosion.buffer);
    prog.grid.erosion.bind_uniform_block("Erosion_data", set.erosion.buffer);
    prog.grid.sediment.bind_uniform_block("Erosion_data", set.erosion.buffer);

    prog.particle.movement.bind_uniform_block("map_settings", set.map.buffer);
    prog.particle.erosion.bind_uniform_block("Erosion_data", set.erosion.buffer);

    return prog;
}

void Erosion::dispatch_grid_rain(Programs& prog, const State::World::Textures& data) {
    prog.grid.rain.use();
    prog.grid.rain.set_uniform("time", data.time);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

// helper functions
void run(Compute_program& program, GLint layer = -1) {
    program.use();
    if (layer != -1) {
        program.set_uniform("t_layer", layer);
    }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(NOISE_SIZE / WRKGRP_SIZE_X, NOISE_SIZE / WRKGRP_SIZE_Y, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
};

void run_particles(Compute_program& program, GLint layer = -1) {
    program.use();
    if (layer != -1) {
        program.set_uniform("t_layer", layer);
    }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDispatchCompute(PARTICLE_COUNT, 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
};

void run_thermal_erosion(Programs& prog, State::World::Textures& data) {
    for (int i = 0; i < SED_LAYERS; i++) {
        run(prog.thermal.flux, i);
        data.thermal_c.swap();
        data.thermal_d.swap();

        run(prog.thermal.transport, i);
        data.heightmap.swap();
    }
}

void dispatch_grid(Programs& prog, State::World::Textures& data) {
    run(prog.grid.flux);
    data.heightmap.swap();
    data.flux.swap();
    data.velocity.swap();

    run(prog.grid.erosion);
    data.heightmap.swap();
    data.velocity.swap();
    data.sediment.swap();

    run_thermal_erosion(prog, data);

    run(prog.grid.sediment);
    data.heightmap.swap();
    data.sediment.swap();

    run(prog.thermal.smooth);
    data.heightmap.swap();
}

void Erosion::dispatch_particle(Programs& prog, State::World::Textures& data) {
    prog.particle.movement.use();
    prog.particle.movement.set_uniform("time", data.time);
    run_particles(prog.particle.movement);
    run_particles(prog.particle.erosion);
    run_thermal_erosion(prog, data);

    // thermal erosion - ravine smoothing
    run(prog.thermal.smooth);
    data.heightmap.swap(true);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

