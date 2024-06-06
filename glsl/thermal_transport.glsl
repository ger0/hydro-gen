#version 460

#include "bindings.glsl"
#line 5

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

layout (binding = BIND_THERMALFLUX_C, rgba32f)   
	uniform coherent image2D thflux_c;

layout (binding = BIND_THERMALFLUX_D, rgba32f)   
	uniform coherent image2D thflux_d;

uniform int t_layer;

vec4 get_thflux(coherent image2D img, ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
       return vec4(0, 0, 0, 0); 
    }
    return imageLoad(img, pos);
}

float sum_flux(vec4 tflux) {
    float sum_tfl = 0;
    for (uint i = 0; i < 4; i++) {
        sum_tfl = tflux[i];        
    }
    return sum_tfl;
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
    terrain[t_layer] += gather_inflow(pos);
    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_heightmap, pos, terrain);
}
