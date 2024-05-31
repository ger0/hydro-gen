#define WORLD_SCALE (0.20)
#define SED_LAYERS (2)

#define LOW_RES_DIV3

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

#define BIND_UNIFORM_CONFIG 0
#define BIND_UNIFORM_EROSION 1
#define BIND_UNIFORM_MAP_SETTINGS 2
#define BIND_UNIFORM_RAIN_SETTINGS 3

#define BIND_STORAGE_MASS_COUNT 4
#define BIND_PARTICLE_BUFFER 5

#if defined(GL_core_profile)
    #define BLOCK layout (std140, binding = BIND_UNIFORM_EROSION) uniform
    #define GL(X) X
    #define FLOAT float
    #define INT int
    #define UINT uint
    #define VEC2 vec2
    #define BOOL bool
#else 
    #define BLOCK struct
    #define FLOAT GLfloat
    #define INT GLint
    #define UINT GLuint
    #define GL(X) alignas(sizeof(X)) X
    #define VEC2 glm::vec2
    #define BOOL GLboolean
#endif

BLOCK Erosion_data {
    GL(FLOAT) Kc;
    GL(FLOAT) Ks;
    GL(FLOAT) Kd;
    GL(FLOAT) Ke;
    GL(FLOAT) G;
    GL(FLOAT) ENERGY_KEPT;
    GL(FLOAT) Kalpha;
    GL(FLOAT) Kspeed;
    GL(FLOAT) d_t;
};

#undef BLOCK

#if defined(GL_core_profile)
    #define BLOCK layout (std140, binding = BIND_UNIFORM_RAIN_SETTINGS) uniform
#else 
    #define BLOCK struct
#endif

struct Rain_data {
    GL(FLOAT)   max_height;
    GL(FLOAT)   amount;
    GL(FLOAT)   mountain_thresh;
    GL(FLOAT)   mountain_multip;
    GL(INT)     period;
    GL(FLOAT)   drops;
};

struct Particle {
    GL(VEC2)    position;
    GL(VEC2)    speed;
    GL(FLOAT)   volume;
    GL(FLOAT)   sediment;
    GL(INT)     iters;
};
