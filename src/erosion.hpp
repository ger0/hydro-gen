
#ifndef HYDR_EROSION_HPP
#define HYDR_EROSION_HPP

#include "shaderprogram.hpp"
#include "state.hpp"
namespace Erosion {

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

struct Particle {
    Compute_program movement;
    Compute_program erosion;
};

struct Grid {
    Compute_program flux;
    Compute_program erosion;
    Compute_program sediment;
    Compute_program rain;
};

struct Thermal {
    Compute_program flux[SED_LAYERS];
    Compute_program transport[SED_LAYERS];
    Compute_program smooth;
};

struct Programs {
    Particle    particle;
    Grid        grid;
    Thermal     thermal;
};

Programs setup_shaders(State::Settings& set, State::World::Textures& data);

void dispatch_grid_rain(Programs& prog, const State::World::Textures& data);
void dispatch_grid(Programs& prog, State::World::Textures& data);
void dispatch_particle(Programs& prog, State::World::Textures& data, bool should_rain);

};
#endif // HYDR_EROSION_HPP
