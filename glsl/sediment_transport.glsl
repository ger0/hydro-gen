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
layout (binding = BIND_WRITE_FLUXMAP, rgba32f)   
	uniform writeonly image2D out_fluxmap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;


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

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 vel = imageLoad(velocitymap, pos);
	vec2 v = vel.xy;
    vec2 back_coords = vec2(pos.x - vel.x * d_t, pos.y - vel.y * d_t);
    float st = get_lerp_sed(back_coords);

    vec4 flux = imageLoad(fluxmap, pos);
    vec4 terrain = imageLoad(heightmap, pos);
    terrain.b *= (1 - Ke * d_t);
    terrain.g = st;
    terrain.w = terrain.r + terrain.b;
    imageStore(out_fluxmap, pos, flux);
    imageStore(out_velocitymap, pos, vel);
    imageStore(out_heightmap, pos, terrain);
}
