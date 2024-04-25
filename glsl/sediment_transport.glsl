#version 460

#include "bindings.glsl"
#line 5

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;

layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;

layout (binding = BIND_THERMALFLUX_C, rgba32f)   
	uniform readonly image2D thflux_c;

layout (binding = BIND_THERMALFLUX_D, rgba32f)   
	uniform readonly image2D thflux_d;

uniform float Ke;
uniform float d_t;

float img_bilinear_g(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos);
    vec2 s_pos = fract(sample_pos);
    float v1 = mix(
        imageLoad(img, ivec2(pos)).g, 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)).g,
        s_pos.x
    );
    float v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)).g, 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)).g,
        s_pos.x
    );
    float value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

float get_lerp_sed(vec2 back_coords) {
    if (back_coords.x < 0.0 || back_coords.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1.0) ||
    back_coords.y < 0.0 || back_coords.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1.0)) {
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

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    vec4 vel = imageLoad(velocitymap, pos);
	vec2 v = vel.xy;
    vec2 back_coords = vec2(pos.x - vel.x * d_t, pos.y - vel.y * d_t);
    float st = get_lerp_sed(back_coords);

    // thermal erosion
    float in_flux = 0.0;
    // cross
    in_flux += get_img(thflux_c, pos + ivec2(-1, 0)).y; // L
    in_flux += get_img(thflux_c, pos + ivec2( 1, 0)).x; // R
    in_flux += get_img(thflux_c, pos + ivec2( 0, 1)).w; // T
    in_flux += get_img(thflux_c, pos + ivec2( 0,-1)).z; // B 

    // diagonal
    in_flux += get_img(thflux_d, pos + ivec2(-1, 1)).w; // LT
    in_flux += get_img(thflux_d, pos + ivec2( 1, 1)).z; // RT
    in_flux += get_img(thflux_d, pos + ivec2(-1,-1)).y; // LB
    in_flux += get_img(thflux_d, pos + ivec2( 1,-1)).x; // RB

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

    vec4 terrain = imageLoad(heightmap, pos);
    terrain.r += sum_flux;
    terrain.g = st;
    terrain.b *= (1 - Ke * d_t);
    terrain.w = terrain.r + terrain.b;
    imageStore(out_heightmap, pos, terrain);
}
