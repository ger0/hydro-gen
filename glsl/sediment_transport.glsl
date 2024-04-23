#version 460

#include "bindings.glsl"
#include "img_interpolation.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;

layout (binding = BIND_WRITE_THERMALFLUX_C, rgba32f)   
	uniform writeonly image2D out_thflux_c;

layout (binding = BIND_WRITE_THERMALFLUX_D, rgba32f)   
	uniform writeonly image2D out_thflux_d;

uniform float max_height;
uniform float d_t;

uniform float Ke;
uniform float Kalpha;
uniform float Kspeed;
const float L = 1.0;
const float a = L;

float get_lerp_sed(vec2 back_coords) {
    if (back_coords.x <= 0.0 || back_coords.x >= (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1.0) ||
    back_coords.y <= 0.0 || back_coords.y >= (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1.0)) {
        return 0.00;
    }
    return img_bilinear_g(heightmap, back_coords);
}

vec4 get_img(readonly image2D img, ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
       return vec4(0, 0, 0, 0); 
    }
    return imageLoad(img, pos);
}

float get_rheight(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return 999999999999.0;
    }
    return imageLoad(heightmap, pos).r;
}

float sum_flux(vec4 tflux) {
    float sum_tfl = 0;
    for (uint i = 0; i < 4; i++) {
        sum_tfl = tflux[i];        
    }
    return sum_tfl;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 vel = imageLoad(velocitymap, pos);
	vec2 v = vel.xy;
    vec2 back_coords = vec2(pos.x - vel.x * d_t, pos.y - vel.y * d_t);
    float st = get_lerp_sed(back_coords);

    vec4 terrain = imageLoad(heightmap, pos);
    terrain.b *= (1 - Ke * d_t);
    terrain.g = st;
    terrain.w = terrain.r + terrain.b;

    // ------------------------ thermal erosion -------------------------
    // total height difference
    vec4 d_h[2];
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

    float S = d_t * Kspeed * a * H / 2.0;
    vec4 out_thfl[2];

    float bk = 0.0;
    // cross
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
            if (alph > Kalpha) {
                bk += b;
                // mark for outflow
                out_thfl[j][i] = 1;
                continue;
            }
            out_thfl[j][i] = 0;
        }
    }

    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            if (out_thfl[j][i] == 1) {
                out_thfl[j][i] = S * d_h[j][i] / bk;
            } else {
                out_thfl[j][i] = 0;
            }
        }
    }

    imageStore(out_heightmap, pos, terrain);
    imageStore(out_thflux_c, pos, out_thfl[0]);
    imageStore(out_thflux_d, pos, out_thfl[1]);
}
