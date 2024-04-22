#version 460

#include "bindings.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;

layout (binding = BIND_WRITE_THERMALFLUX_C, rgba32f)   
	uniform writeonly image2D out_thflux_c;

layout (binding = BIND_WRITE_THERMALFLUX_D, rgba32f)   
	uniform writeonly image2D out_thflux_d;

const float L = 1;

uniform float Ki;
uniform float a = L * L;
uniform float Kt;

float get_rheight(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return 999999999999.0;
    }
    return imageLoad(heightmap, pos).r;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    // thermal erosion
    // total height difference
    vec4 d_h[2];
    float terrain = get_rheight(pos);
    // cross
    d_h[0].x = terrain.r - get_rheight(pos + ivec2(-1, 0)); // L
    d_h[0].y = terrain.r - get_rheight(pos + ivec2( 1, 0)); // R
    d_h[0].z = terrain.r - get_rheight(pos + ivec2( 0, 1)); // T
    d_h[0].w = terrain.r - get_rheight(pos + ivec2( 0,-1)); // B

    // diagonal
    d_h[1].x = terrain.r - get_rheight(pos + ivec2(-1, 1)); // LT
    d_h[1].y = terrain.r - get_rheight(pos + ivec2( 1, 1)); // RT
    d_h[1].z = terrain.r - get_rheight(pos + ivec2(-1,-1)); // LB
    d_h[1].w = terrain.r - get_rheight(pos + ivec2( 1,-1)); // RB

    float H = 0;
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            if (d_h[j][i] > H) {
                H = d_h[j][i];
            }
        }
    }
    float S = a * H / 2.0;
    vec4 out_thfl[2];

    float bk;
    // cross
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            float b = d_h[0][i];
            if (b <= 0) continue;
            float d = L;
            // diagonal
            if (j == 1) d *= sqrt(2);
            float alph = tan(b) / d;
            if (alph <= Ki) {
                bk += b;
                // mark for outflow
                out_thfl[j][i] = 1;
            }
        }
    }
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            if (out_thfl[j][i] != 1) continue;

            out_thfl[j][i] = S * d_h[j][i] / bk;
        }
    }
    imageStore(out_thflux_c, pos, out_thfl[0]);
    imageStore(out_thflux_d, pos, out_thfl[1]);
}
