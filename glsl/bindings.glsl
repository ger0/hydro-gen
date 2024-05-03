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

#define BIND_UNIFORM_CONFIG 0
#define BIND_UNIFORM_EROSION 1
#define BIND_UNIFORM_MAP_SETTINGS 2
#define BIND_UNIFORM_RAIN_SETTINGS 3

#if defined(GL_core_profile)
    #define BLOCK layout (std140, binding = BIND_UNIFORM_EROSION) uniform
    #define GL(X) X
    #define FLOAT float
    #define INT int
#else 
    #define BLOCK struct
    #define FLOAT GLfloat
    #define INT GLint
    #define GL(X) alignas(sizeof(X)) X
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
    #define GL(X) X
    #define FLOAT float
    #define INT int
#else 
    #define BLOCK struct
    #define FLOAT GLfloat
    #define INT GLint
    #define GL(X) alignas(sizeof(X)) X
#endif

struct Rain_data {
    GL(FLOAT)   max_height;
    GL(FLOAT)   amount;
    GL(FLOAT)   mountain_thresh;
    GL(FLOAT)   mountain_multip;
    GL(INT)     period;
    GL(FLOAT)   drops;
};

#undef FLOAT
#undef INT
#undef GL

