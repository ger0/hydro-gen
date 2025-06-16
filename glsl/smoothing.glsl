// based on: https://github.com/Huw-man/Interactive-Erosion-Simulator-on-GPU/
#version 460 core

#include <bindings>
#line 6
layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = 3, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = 4, rgba32f)   
	uniform writeonly image2D out_heightmap;

layout (std140, binding = BIND_UNIFORM_EROSION) uniform erosion_data {
    Erosion_data set;
};

#if defined(PARTICLE_COUNT)
layout (binding = 5, rgba32f)   
	uniform image2D momentmap;
layout (binding = 6, rgba32f)   
	uniform image2D out_momentmap;
#endif

void main() {
    float d_time = set.d_t;
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 terrain = imageLoad(heightmap, pos);
    vec2 terr = terrain.rg;
    if (pos.x == 0 || pos.y == 0 
        || pos.x == (gl_NumWorkGroups.x * WRKGRP_SIZE_X - 1)
        || pos.y == (gl_NumWorkGroups.y * WRKGRP_SIZE_Y - 1)
    ) {
        imageStore(out_heightmap, pos, terrain);
        return;
    }
    vec2 l = imageLoad(heightmap, pos + ivec2(-1, 0)).rg;
    vec2 r = imageLoad(heightmap, pos + ivec2( 1, 0)).rg;
    vec2 t = imageLoad(heightmap, pos + ivec2( 0, 1)).rg;
    vec2 b = imageLoad(heightmap, pos + ivec2( 0,-1)).rg;

	vec2 d_l = terr - l;
	d_l.g += d_l.r;
	vec2 d_r = terr - r;
	d_r.g += d_r.r;
    
	vec2 d_t = terr - t;
	d_t.g += d_t.r;
	vec2 d_b = terr - b;
	d_b.g += d_b.r;

    float g_hdiff = (d_l.g + d_r.g + d_t.g + d_b.g) / (4.0);
    float r_hdiff = (d_l.r + d_r.r + d_t.r + d_b.r) / (4.0);

    g_hdiff = abs(g_hdiff);
    // g_hdiff = set.Kalpha.g / 2.0;
    r_hdiff = abs(r_hdiff);
    // r_hdiff = set.Kalpha.r / 2.0;

    // Only do this at a minimum/maximum -- we can tell we're at a min/max if the sign of the derivative suddenly changes
    // In this case, it happens when they're the same, since the vectors are pointing in different directions
    vec2 x_crv = d_l * d_r;
    vec2 y_crv = d_t * d_b;

    if ((((-d_l.r) > r_hdiff || (-d_r.r) > r_hdiff) && x_crv.r > 0)
        || (((-d_t.r) > r_hdiff || (-d_b.r) > r_hdiff) && y_crv.r > 0)
    ) {
        terr.r = (terr.r + l.r + r.r + t.r + b.r) / 5.0; // Set height to average
    }

    if ((((-d_l.g) > g_hdiff || (-d_r.g) > g_hdiff) && x_crv.g > 0)
        || (((-d_t.g) > g_hdiff || (-d_b.g) > g_hdiff) && y_crv.g > 0)
    ) {
        terr.g = (terr.g + l.g + r.g + t.g + b.g) / 5.0; // Set height to average
    }

    float multip = clamp(set.Kspeed[1] * d_time, 0, 1);
    // water display on particles
#if defined(PARTICLE_COUNT)
    vec4 momentum = imageLoad(momentmap, pos);
    momentum.xy *= clamp(1 - (1e-12 * PARTICLE_COUNT), 0, 1);
    momentum.xy += (1e-12 * PARTICLE_COUNT) * momentum.zw;
    momentum.zw = vec2(0);

    terrain.b *= clamp(1 - (8e-8 * PARTICLE_COUNT), 0, 1);
    if (pos.x == 0 || pos.y == 0 || 
        pos.x == (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
        pos.y == (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1) ||
        terrain.b < 1e-6
    ) {
        terrain.b = 0;
    }
    if (length(momentum.xy) < 1e-12) {
        momentum.xy = vec2(0);
    }
    imageStore(out_momentmap, pos, momentum);
    multip = clamp(set.Kspeed[1] * d_time, 0, 1);
#endif

    // multip = 1.0;

    terrain.rg = multip * terr.rg + (1 - multip) * terrain.rg;

    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_heightmap, pos, terrain);
}
