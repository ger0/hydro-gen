// based on: https://github.com/Huw-man/Interactive-Erosion-Simulator-on-GPU/
#version 460 core

#include "bindings.glsl"
#line 6
layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

void main() {
    float d_time = d_t;
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
    float m_hdiff = tan(Kalpha);

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

    // Only do this at a minimum/maximum -- we can tell we're at a min/max if the sign of the derivative suddenly changes
    // In this case, it happens when they're the same, since the vectors are pointing in different directions
    vec2 x_crv = d_l * d_r;
    vec2 y_crv = d_t * d_b;

    if (((abs(d_l.r) > m_hdiff || abs(d_r.r) > m_hdiff) && x_crv.r > 0)
        || ((abs(d_t.r) > m_hdiff || abs(d_b.r) > m_hdiff) && y_crv.r > 0)
    ) {
        terr.r = (terr.r + l.r + r.r + t.r + b.r) / 5.0; // Set height to average
    }

    if (((abs(d_l.g) > m_hdiff || abs(d_r.g) > m_hdiff) && x_crv.g > 0)
        || ((abs(d_t.g) > m_hdiff || abs(d_b.g) > m_hdiff) && y_crv.g > 0)
    ) {
        terr.g = (terr.g + l.g + r.g + t.g + b.g) / 5.0; // Set height to average
    }
    float multip = Kspeed * d_time;
    terrain.rg = multip * terr.rg + (1 - multip) * terrain.rg;
    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_heightmap, pos, terrain);
}
