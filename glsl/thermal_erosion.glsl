#version 460

#include <bindings>
#line 4

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = 3) uniform sampler2D heightmap;
layout (binding = 1, rgba32f) uniform writeonly image2D out_thflux_c;
layout (binding = 2, rgba32f) uniform writeonly image2D out_thflux_d;

layout (std140, binding = BIND_UNIFORM_EROSION) uniform erosion_data {
    Erosion_data set;
};

uniform int t_layer;

const float a = L;

vec4 get_height(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return vec4(999999999999.0);
    }
    return texelFetch(heightmap, pos, 0);
}

void store_outflow(ivec2 pos, vec4 terrain, int layer) {
    // total height difference
    vec4 d_h[2] = {vec4(0), vec4(0)};

    for (int i = 0; i <= layer; i++) {
        // cross
        d_h[0].x += terrain[i] - get_height(pos + ivec2(-1, 0))[i]; // L
        d_h[0].y += terrain[i] - get_height(pos + ivec2( 1, 0))[i]; // R
        d_h[0].z += terrain[i] - get_height(pos + ivec2( 0, 1))[i]; // T
        d_h[0].w += terrain[i] - get_height(pos + ivec2( 0,-1))[i]; // B
                           
        // diagonal        
        d_h[1].x += terrain[i] - get_height(pos + ivec2(-1, 1))[i]; // LT
        d_h[1].y += terrain[i] - get_height(pos + ivec2( 1, 1))[i]; // RT
        d_h[1].z += terrain[i] - get_height(pos + ivec2(-1,-1))[i]; // LB
        d_h[1].w += terrain[i] - get_height(pos + ivec2( 1,-1))[i]; // RB
    }

    float H = 0;
    // uint imax, jmax;
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            if (d_h[j][i] > H) {
                H = d_h[j][i];
                /*
                imax = i;
                jmax = j; */
            }
        }
    }
    H = min(terrain[layer], H);
    vec4 out_thfl[2] = {vec4(0), vec4(0)};
    float bk = 0.0;

    float sharpness = 1.0;
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            float b = d_h[j][i];
            if (b <= 0) {
                out_thfl[j][i] = 0;
                continue;
            }
            float d = L;
            // diagonal
            if (j == 1) {
                d *= sqrt(2.0);
            }
            float alph = atan(b / (d / WORLD_SCALE));
            float Kl_alph = set.Kalpha[layer];
            // float Kl_alph = set.Kalpha;
            if (alph > Kl_alph) {
                // speed up when the angle is too big
                float newsh = 1.0 + alph - Kl_alph;
                if (newsh > sharpness) {
                    sharpness = newsh;
                }
                bk += b;
                // mark for outflow
                out_thfl[j][i] = 1;
                continue;
            }
            out_thfl[j][i] = 0;
        }
    }
    sharpness *= sharpness * sharpness;
    float Klspeed = set.Kspeed[layer];

    float S = set.d_t * Klspeed * sharpness * a * H / 2.0;
    // float S = set.d_t * set.Kspeed * a * H / 2.0;

    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            if (out_thfl[j][i] == 1) {
                out_thfl[j][i] = S * d_h[j][i] / bk;
            } else {
                out_thfl[j][i] = 0;
            }
        }
    }
    imageStore(out_thflux_c, pos, out_thfl[0]);
    imageStore(out_thflux_d, pos, out_thfl[1]);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 terrain = texelFetch(heightmap, pos, 0);
    store_outflow(pos, terrain, t_layer);
}
