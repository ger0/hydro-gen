
#ifndef HYDR_EROSION_HPP
#define HYDR_EROSION_HPP

#include "shaderprogram.hpp"
#include "state.hpp"
namespace Erosion {

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

Programs* setup_shaders(State::Settings& set, State::World::Textures& data);

void dispatch_grid_rain(Programs& prog, State::World::Textures& data);
void dispatch_grid(Programs& prog, State::World::Textures& data);
void dispatch_particle(Programs& prog, State::World::Textures& data, bool should_rain);

};
#endif // HYDR_EROSION_HPP
