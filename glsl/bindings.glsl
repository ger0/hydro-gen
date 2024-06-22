#ifndef HYDR_GL_BINDINGS_HPP
#define HYDR_GL_BINDINGS_HPP

#define WORLD_SCALE (1.00)
#define SED_LAYERS (2)

// #define PARTICLE_COUNT (262144)
// #define PARTICLE_COUNT (1048576)

// #define LOW_RES_DIV3

#define WRKGRP_SIZE_X 8
#define WRKGRP_SIZE_Y 8

#define BIND_DISPLAY_TEXTURE 0

#define BIND_HEIGHTMAP 1
#define BIND_FLUXMAP 2
#define BIND_VELOCITYMAP 3 
#define BIND_SEDIMENTMAP 4 

#define BIND_THERMALFLUX_C 5 
#define BIND_THERMALFLUX_D 6 

#define BIND_WRITE_HEIGHTMAP 7 
#define BIND_WRITE_FLUXMAP 8 
#define BIND_WRITE_VELOCITYMAP 9
#define BIND_WRITE_SEDIMENTMAP 10

#define BIND_WRITE_THERMALFLUX_C 11
#define BIND_WRITE_THERMALFLUX_D 12

#define BIND_LOCKMAP 13

#define BIND_UNIFORM_EROSION 1
#define BIND_UNIFORM_MAP_SETTINGS 2
#define BIND_UNIFORM_RAIN_SETTINGS 3
#define BIND_PARTICLE_BUFFER 4

#if defined(GL_core_profile) 
    const float L = 1.0;
#endif

#if defined(GL_core_profile)
    #define BLOCK layout (std140, binding = BIND_UNIFORM_EROSION) uniform
    #define GL(X) X
    #define FLOAT float
    #define INT int
    #define UINT uint
    #define VEC2 vec2
    #define IVEC2 ivec2
    #define BOOL bool
#else 
    #define BLOCK struct
    #define FLOAT GLfloat
    #define INT GLint
    #define UINT GLuint
    #define GL(X) alignas(sizeof(X)) X
    #define VEC2 glm::vec2
    #define IVEC2 glm::ivec2
    #define BOOL GLboolean
#endif

BLOCK Erosion_data {
    GL(FLOAT) Kc;
    GL(VEC2) Ks;
    GL(VEC2) Kd;
    GL(FLOAT) Ke;
    GL(VEC2) Kalpha;
    GL(FLOAT) ENERGY_KEPT;
    GL(VEC2) Kspeed;
    GL(FLOAT) G;
    GL(FLOAT) d_t;

    GL(FLOAT) density;
    GL(FLOAT) init_volume;
    GL(FLOAT) friction;
    GL(FLOAT) inertia;
    GL(FLOAT) min_volume;
    GL(FLOAT) min_velocity;
    GL(UINT)  ttl; // time to live
};

#undef BLOCK

#if defined(GL_core_profile)
    #define BLOCK layout (std140, binding = BIND_UNIFORM_RAIN_SETTINGS) uniform
#else 
    #define BLOCK struct
#endif

struct Rain_data {
    GL(FLOAT)   amount;
    GL(FLOAT)   mountain_thresh;
    GL(FLOAT)   mountain_multip;
    GL(INT)     period;
    GL(FLOAT)   drops;
};

struct Map_settings_data {
    GL(FLOAT) max_height;

    GL(FLOAT) max_dirt;

    GL(IVEC2) hmap_dims;
    GL(FLOAT) height_mult;
    GL(FLOAT) water_lvl;
    GL(FLOAT) seed;
    GL(FLOAT) persistance;
    GL(FLOAT) lacunarity;
    GL(FLOAT) scale;
    GL(FLOAT) redistribution;
    GL(INT)   octaves;

    GL(UINT)  fake_erosion;

    GL(UINT)  mask_round;
    GL(UINT)  mask_exp;
    GL(UINT)  mask_power;
    GL(UINT)  mask_slope;

    GL(UINT)  uplift;
    GL(FLOAT) uplift_scale;

    GL(INT)   domain_warp;
    GL(FLOAT) domain_warp_scale;
    GL(INT)   terrace;
    GL(FLOAT) terrace_scale;
};

struct Particle {
    GL(FLOAT)   sc;
    GL(INT)     iters;
    GL(VEC2)    position;
    GL(VEC2)    velocity;
    GL(FLOAT)   volume;
    GL(VEC2)    sediment;
    // sediment capacity at a point
    // sediment layers
    GL(BOOL)    to_kill;
};
#endif // HYDR_GL_BINDINGS_HPP``
