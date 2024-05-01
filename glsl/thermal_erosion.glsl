#version 460

#include "bindings.glsl"
#line 5

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform coherent image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

layout (binding = BIND_THERMALFLUX_C, rgba32f)   
	uniform coherent image2D thflux_c;

layout (binding = BIND_THERMALFLUX_D, rgba32f)   
	uniform coherent image2D thflux_d;

/* layout (binding = BIND_WRITE_THERMALFLUX_C, rgba32f)   
	uniform writeonly image2D out_thflux_c;

layout (binding = BIND_WRITE_THERMALFLUX_D, rgba32f)   
	uniform writeonly image2D out_thflux_d; */

uniform float max_height;
uniform float d_t;

uniform float Ke;
uniform float Kalpha;
uniform float Kspeed;
uniform int t_layer;

const float L = 1.0;
const float a = L;

vec4 get_thflux(coherent image2D img, ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
       return vec4(0, 0, 0, 0); 
    }
    return imageLoad(img, pos);
}

vec4 get_height(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return vec4(999999999999.0);
    }
    return imageLoad(heightmap, pos);
}

float sum_flux(vec4 tflux) {
    float sum_tfl = 0;
    for (uint i = 0; i < 4; i++) {
        sum_tfl = tflux[i];        
    }
    return sum_tfl;
}

void calculate_outflow(ivec2 pos, vec4 terrain, int layer) {
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
    // getting the height difference for ONE LAYER ONLY
    // H = d_layerh[jmax][imax];
    H = min(terrain[layer], H);

    vec4 out_thfl[2] = {vec4(0), vec4(0)};
    //float layer_total = 0.0;
    float bk = 0.0;

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
            float Kl_alph = Kalpha / float(layer + 1);
            // float Kl_alph = Kalpha;
            if (alph > Kl_alph) {
                // float newsh = 1.0 + alph - Kl_alph;
                /* if (newsh > sharpness) {
                    sharpness = newsh;
                }
                 */
                bk += b;
                // mark for outflow
                out_thfl[j][i] = 1;
                continue;
            }
            out_thfl[j][i] = 0;
        }
    }
    // sharpness *= sharpness * sharpness;
    // float S = d_t * Kspeed * sharpness * a * H / 2.0;
    float Klspeed = Kspeed * float(layer + 1);
    float S = d_t * Kspeed * a * H / 2.0;

    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            if (out_thfl[j][i] == 1) {
                out_thfl[j][i] = S * d_h[j][i] / bk;
            } else {
                out_thfl[j][i] = 0;
            }
        }
    }
    imageStore(thflux_c, pos, out_thfl[0]);
    imageStore(thflux_d, pos, out_thfl[1]);
}

float gather_inflow(ivec2 pos) {
    // thermal erosion
    float in_flux = 0.0;
    // cross
    in_flux += get_thflux(thflux_c, pos + ivec2(-1, 0)).y; // L
    in_flux += get_thflux(thflux_c, pos + ivec2( 1, 0)).x; // R
    in_flux += get_thflux(thflux_c, pos + ivec2( 0, 1)).w; // T
    in_flux += get_thflux(thflux_c, pos + ivec2( 0,-1)).z; // B 

    // diagonal
    in_flux += get_thflux(thflux_d, pos + ivec2(-1, 1)).w; // LT
    in_flux += get_thflux(thflux_d, pos + ivec2( 1, 1)).z; // RT
    in_flux += get_thflux(thflux_d, pos + ivec2(-1,-1)).y; // LB
    in_flux += get_thflux(thflux_d, pos + ivec2( 1,-1)).x; // RB

    float sum_flux = 0.0;
    vec4 out_flux[2];
    out_flux[0] = imageLoad(thflux_c, pos);
    out_flux[1] = imageLoad(thflux_d, pos);
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            sum_flux -= out_flux[j][i];
        }
    }
    sum_flux += in_flux;
    return sum_flux;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 terrain = imageLoad(heightmap, pos);

    // ------------------------ thermal erosion -------------------------
    calculate_outflow(pos, terrain, t_layer);
    memoryBarrierImage();
    barrier();

    terrain[t_layer] += gather_inflow(pos);
    imageStore(heightmap, pos, terrain);
    imageStore(thflux_c, pos, vec4(0));
    imageStore(thflux_d, pos, vec4(0));
    memoryBarrierImage();
    barrier();

    imageStore(out_heightmap, pos, terrain);
}
