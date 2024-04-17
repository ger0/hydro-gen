#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
layout (local_size_x = 8, local_size_y = 8) in;

// (dirt, rock, water, total)
layout (binding = BIND_HEIGHTMAP, rgba32f)
    uniform writeonly image2D dest_tex;

layout (binding = BIND_FLUXMAP, rgba32f)
    uniform writeonly image2D flux_tex;

layout (binding = BIND_VELOCITYMAP, rgba32f)
    uniform writeonly image2D vel_tex;

layout (binding = BIND_WATER_TEXTURE, r32f)
    uniform writeonly image2D water_tex;

uniform float height_scale;
uniform float water_lvl;

// Function to generate a random float in the range [0, 1]
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    ivec2 store_pos = ivec2(gl_GlobalInvocationID.xy);

    gln_tFBMOpts opts = gln_tFBMOpts(0, 0.5, 2.0, 0.0070, 1, 8, false, false);
    float val = (gln_sfbm(store_pos, opts) + 1) / 2;
    vec4 terrain = vec4(val * height_scale, 0.0, 0.0, 0.0);
    float water_col = gln_simplex(vec2(store_pos));
    // water height
    terrain.b += 0.4;
    terrain.w = terrain.r + terrain.b;

    imageStore(dest_tex, store_pos, terrain);
    imageStore(water_tex, store_pos, vec4(water_col, 0, 0, 0));
    imageStore(vel_tex, store_pos, vec4(0,0,0,0));
    imageStore(flux_tex, store_pos, vec4(0,0,0,0));
}
