#version 460

#include "img_interpolation.glsl"
#include "bindings.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

// (fL, fR, fT, fB) left, right, top, bottom
layout (binding = BIND_FLUXMAP, rgba32f)   
	uniform readonly image2D fluxmap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;

layout (binding = BIND_THERMALFLUX_C, rgba32f)   
	uniform readonly image2D thflux_c;

layout (binding = BIND_THERMALFLUX_D, rgba32f)   
	uniform readonly image2D thflux_d;


uniform float max_height;
uniform float d_t;

uniform float Ke;

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

    // thermal erosion
    vec4 tfl[2];

    // cross
    tfl[0].x = get_img(thflux_c, pos + ivec2(-1, 0)).y; // L
    tfl[0].y = get_img(thflux_c, pos + ivec2( 1, 0)).x; // R
    tfl[0].z = get_img(thflux_c, pos + ivec2( 0, 1)).w; // T
    tfl[0].w = get_img(thflux_c, pos + ivec2( 0,-1)).z; // B 

    // diagonal
    tfl[1].x = get_img(thflux_d, pos + ivec2(-1, 1)).w; // LT
    tfl[1].y = get_img(thflux_d, pos + ivec2( 1, 1)).z; // RT
    tfl[1].z = get_img(thflux_d, pos + ivec2(-1,-1)).y; // LB
    tfl[1].w = get_img(thflux_d, pos + ivec2( 1,-1)).x; // RB

    float sum_tfl = 0;
    for (uint j = 0; j < 2; j++) {
        for (uint i = 0; i < 4; i++) {
            sum_tfl += tfl[j][i];
        }
    }
    terrain.r += sum_tfl;
    terrain.b *= (1 - Ke * d_t);
    terrain.g = st;
    terrain.w = terrain.r + terrain.b;
    imageStore(out_heightmap, pos, terrain);
}
