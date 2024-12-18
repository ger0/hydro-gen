#version 460

#include "bindings.glsl"
#line 5

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP) uniform sampler2D heightmap;

layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP) uniform sampler2D velocitymap;

layout (binding = BIND_SEDIMENTMAP) uniform sampler2D sedimap;
layout (binding = BIND_WRITE_SEDIMENTMAP, rgba32f)   
	uniform writeonly image2D out_sedimap;

layout (std140, binding = BIND_UNIFORM_EROSION) uniform erosion_data {
    Erosion_data set;
};

vec2 pos_to_uv(vec2 pos) {
    return vec2(
        pos.x / float(gl_WorkGroupSize.x * gl_NumWorkGroups.x),
        pos.y / float(gl_WorkGroupSize.y * gl_NumWorkGroups.y)
    );
}

vec4 get_lerp_sed(vec2 back_coords) {
    back_coords.x = clamp(back_coords.x, 0, gl_NumWorkGroups.x * WRKGRP_SIZE_X - 1);
    back_coords.y = clamp(back_coords.y, 0, gl_NumWorkGroups.y * WRKGRP_SIZE_Y - 1);
    return texture(sedimap, pos_to_uv(back_coords));
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
vec2 mac_cormack_backward(vec2 currentCoords, sampler2D velocityField, float dt) {
    // Forward advection
    vec2 velocity = texture(velocityField, pos_to_uv(currentCoords)).xy;
    vec2 advectedCoords = advect_coords(currentCoords, velocity, dt);

    // Backward advection
    vec2 advectedVelocity = texture(velocityField, pos_to_uv(advectedCoords)).xy;
    vec2 correctorCoords = advect_coords(advectedCoords, -advectedVelocity, dt);

    return (advectedCoords) + (currentCoords - correctorCoords) / 2.0;
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

    vec4 vel = texelFetch(velocitymap, pos, 0);
    vec2 back_coords = vec2(pos.x - vel.x * set.d_t, pos.y - vel.y * set.d_t);
    // vec2 back_coords = mac_cormack_backward(gl_GlobalInvocationID.xy, velocitymap, set.d_t);
    vec4 st = get_lerp_sed(back_coords);

    vec4 terrain = texelFetch(heightmap, pos, 0);
    terrain.b *= (1 - set.Ke * set.d_t);

    if (vel.z == 0) {
        terrain.rg += st.rg;
        st.rg = vec2(0);
    }

    /* // NEW, some sediment gets deposited on water evaporation
    vec2 d_st = st.rg * vec2(set.Ke * set.d_t);
    terrain.r += d_st.r;
    terrain.g += d_st.g;

    st.r -= d_st.r;
    st.g -= d_st.g; */

    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_sedimap, pos, st);
    imageStore(out_heightmap, pos, terrain);
}
