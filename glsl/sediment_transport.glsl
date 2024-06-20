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
    return img_bilinear(sedimap, back_coords);
}

vec4 get_img(readonly image2D img, ivec2 pos) {
    return imageLoad(img, pos);
}

vec2 advect_coords(vec2 coords, vec2 vel, float d_t) {
    vec2 adv = coords - vel * d_t;
    vec2 max_dims = vec2(
        gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1,
        gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1
    );
    if (adv.x < 0) {
        adv.x = 0;
    } else if (adv.x > max_dims.x) {
        adv.x = max_dims.x;
    }

    if (adv.y < 0) {
        adv.y = 0;
    } else if (adv.y > max_dims.y) {
        adv.y = max_dims.y;
    }

    return adv;
}

// Semi-Lagrangian MacCormack method for backward advection
vec2 mac_cormack_backward(vec2 currentCoords, readonly image2D velocityField, float dt) {
    // Forward advection
    vec2 velocity = img_bilinear(velocityField, currentCoords).xy;
    vec2 advectedCoords = advect_coords(currentCoords, velocity, dt);

    // Backward advection
    vec2 advectedVelocity = img_bilinear(velocityField, advectedCoords).xy;
    vec2 correctorCoords = advect_coords(advectedCoords, -advectedVelocity, dt);

    return advectedCoords + (currentCoords - correctorCoords) / 2.0;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    // vec4 vel = imageLoad(velocitymap, pos);
    // vec2 back_coords = vec2(pos.x - vel.x * d_t, pos.y - vel.y * d_t);
    vec2 back_coords = mac_cormack_backward(gl_GlobalInvocationID.xy, velocitymap, d_t);
    vec4 st = get_lerp_sed(back_coords);

    vec4 terrain = imageLoad(heightmap, pos);
    terrain.b *= (1 - Ke * d_t);
    // deposit all sediment when there's no water (NEW)
    if (terrain.b < 1e-09) {
        terrain.r += st.r;
        terrain.g += st.g;
        st.r = 0;
        st.g = 0;
    }
    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_sedimap, pos, st);
    imageStore(out_heightmap, pos, terrain);
}
