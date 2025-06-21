#include "erosion.hpp"
using namespace Erosion;

// shader filenames

// grid based
constexpr auto grid_rain_comput_file    = "rain.glsl";
constexpr auto grid_hydro_flux_file     = "hydro_flux.glsl";
constexpr auto grid_hydro_erosion_file  = "hydro_erosion.glsl";
constexpr auto grid_sediment_file       = "sediment_transport.glsl";

// thermal erosion - grid based
constexpr auto thermal_flux_file        = "thermal_erosion.glsl";
constexpr auto thermal_transport_file   = "thermal_transport.glsl";
constexpr auto smooth_file              = "smoothing.glsl";

// particle based
constexpr auto particle_move_file       = "particle.glsl";
constexpr auto particle_erosion_file    = "particle_erosion.glsl";

Programs* Erosion::setup_shaders(
        Programs::Erosion_type type, 
        State::Settings& set,
        State::World::Textures& data, 
        u32 particle_count
) {
    // compile compute shaders
    auto prog = new Programs{
        type,
        type == Programs::PARTICLES ? new Particle{
            .movement   = Compute_program(particle_move_file),
            .erosion    = Compute_program(particle_erosion_file)
        } : nullptr,
        type == Programs::GRID ? new Grid{
            .flux       = Compute_program(grid_hydro_flux_file),
            .erosion    = Compute_program(grid_hydro_erosion_file),
            .sediment   = Compute_program(grid_sediment_file),
            .rain       = Compute_program(grid_rain_comput_file)
        } : nullptr,
        Thermal{
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
        prog->thermal.flux[i].use();
        prog->thermal.flux[i].bind_uniform_block("erosion_data", set.erosion.buffer);
        prog->thermal.flux[i].set_uniform("t_layer", i);
        prog->thermal.transport[i].use();
        prog->thermal.transport[i].set_uniform("t_layer", i);
        glUseProgram(0);
    }

    if (prog->grid != nullptr) {
        prog->grid->flux.bind_uniform_block("erosion_data", set.erosion.buffer);
        prog->grid->erosion.bind_uniform_block("erosion_data", set.erosion.buffer);
        prog->grid->sediment.bind_uniform_block("erosion_data", set.erosion.buffer);
    } 
    if (prog->particle != nullptr) {
        prog->particle->movement.bind_uniform_block("map_settings", set.map.buffer);
        prog->particle->erosion.bind_uniform_block("erosion_data", set.erosion.buffer);
        prog->particle->movement.bind_storage_buffer("ParticleBuffer", data.particle_buffer);
        prog->particle->erosion.bind_storage_buffer("ParticleBuffer", data.particle_buffer);
    }
    return prog;
}

void Erosion::dispatch_grid_rain(Programs& prog, State::World::Textures& data) {
    prog.grid->rain.use();
    prog.grid->rain.set_uniform("time", data.time);
    prog.grid->rain.bind_image("heightmap", data.heightmap.get_read_tex());
    prog.grid->rain.bind_image("out_heightmap", data.heightmap.get_write_tex());
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(
        State::NOISE_SIZE / WRKGRP_SIZE_X,
        State::NOISE_SIZE / WRKGRP_SIZE_Y, 
        1
    );
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    data.heightmap.swap();
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

void run_particles(Compute_program& program, u32 particle_count) {
    program.use();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDispatchCompute(particle_count / (WRKGRP_SIZE_X * WRKGRP_SIZE_Y), 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
};

void Erosion::dispatch_particle(Programs& prog, State::World::Textures& data, bool should_rain) {
    prog.particle->movement.use();
    prog.particle->movement.set_uniform("time", data.time);
    prog.particle->movement.set_uniform("should_rain", should_rain);
    prog.particle->movement.bind_texture("heightmap", data.heightmap.get_read_tex());
    prog.particle->movement.bind_texture("momentmap", data.velocity.get_read_tex());
    run_particles(prog.particle->movement, prog.particle->count);

    prog.particle->erosion.use();
    prog.particle->erosion.bind_image("lockmap", data.lockmap);
    prog.particle->erosion.bind_image("heightmap", data.heightmap.get_read_tex());
    prog.particle->erosion.bind_image("momentmap", data.velocity.get_read_tex());
    run_particles(prog.particle->erosion, prog.particle->count);

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

void Erosion::dispatch_grid(Programs& prog, State::World::Textures& data) {
    prog.grid->flux.use();
    prog.grid->flux.bind_texture("heightmap", data.heightmap.get_read_tex());
    prog.grid->flux.bind_texture("fluxmap", data.flux.get_read_tex());
    prog.grid->flux.bind_texture("velocitymap", data.velocity.get_read_tex());
    prog.grid->flux.bind_image("out_heightmap", data.heightmap.get_write_tex());
    prog.grid->flux.bind_image("out_fluxmap", data.flux.get_write_tex());
    prog.grid->flux.bind_image("out_velocitymap", data.velocity.get_write_tex());
    run(prog.grid->flux);
    data.heightmap.swap();
    data.flux.swap();
    data.velocity.swap();

    prog.grid->erosion.use();
    prog.grid->erosion.bind_image("heightmap", data.heightmap.get_read_tex());
    prog.grid->erosion.bind_image("sedimap", data.sediment.get_read_tex());
    prog.grid->erosion.bind_image("velocitymap", data.velocity.get_read_tex());
    prog.grid->erosion.bind_image("out_heightmap", data.heightmap.get_write_tex());
    prog.grid->erosion.bind_image("out_sedimap", data.sediment.get_write_tex());
    run(prog.grid->erosion);
    data.heightmap.swap();
    data.sediment.swap();

    prog.grid->sediment.use();
    prog.grid->sediment.bind_texture("heightmap", data.heightmap.get_read_tex());
    prog.grid->sediment.bind_texture("velocitymap", data.velocity.get_read_tex());
    prog.grid->sediment.bind_texture("sedimap", data.sediment.get_read_tex());
    prog.grid->sediment.bind_image("out_heightmap", data.heightmap.get_write_tex());
    prog.grid->sediment.bind_image("out_sedimap", data.sediment.get_write_tex());
    run(prog.grid->sediment);
    data.heightmap.swap();
    data.sediment.swap();

    run_thermal_erosion(prog, data);

    prog.thermal.smooth.use();
    prog.thermal.smooth.bind_image("heightmap", data.heightmap.get_read_tex());
    prog.thermal.smooth.bind_image("out_heightmap", data.heightmap.get_write_tex());
    prog.thermal.smooth.unbind_image("momentmap");
    prog.thermal.smooth.unbind_image("out_momentmap");
    run(prog.thermal.smooth);
    data.heightmap.swap();
}
