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

layout (binding = BIND_SEDIMENTMAP, rgba32f)   
	uniform readonly image2D sedimap;
layout (binding = BIND_WRITE_SEDIMENTMAP, rgba32f)   
	uniform writeonly image2D out_sedimap;

uniform float Ke;
uniform float d_t;

vec4 img_bilinear(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos);
    vec2 s_pos = fract(sample_pos);
    vec4 v1 = mix(
        imageLoad(img, ivec2(pos)), 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)),
        s_pos.x
    );
    vec4 v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)), 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)),
        s_pos.x
    );
    vec4 value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

vec4 get_lerp_sed(vec2 back_coords) {
    if (back_coords.x < 0.0 || back_coords.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1.0) ||
    back_coords.y < 0.0 || back_coords.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1.0)) {
        return vec4(0.0);
    }
    return img_bilinear(sedimap, back_coords);
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
    vec4 st = get_lerp_sed(back_coords);

    vec4 terrain = imageLoad(heightmap, pos);
    terrain.b *= (1 - Ke * d_t);
    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_sedimap, pos, st);
    imageStore(out_heightmap, pos, terrain);
}
