#version 460

#include "bindings.glsl"
#line 5

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

// (dirt height, rock height, water height, total height)
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
// vec3((u, v), )
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;

// cross-section area of a pipe
const float A = 1.0;

float get_wheight(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        return 999999999999.0;
    }
    return imageLoad(heightmap, pos).w;
}

vec4 get_flux(ivec2 pos) {
    if (pos.x < 0 || pos.x > (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y < 0 || pos.y > (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
       return vec4(0, 0, 0, 0); 
    }
    return imageLoad(fluxmap, pos);
}

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

vec2 get_lerp_vel(vec2 back_coords) {
    back_coords.x = clamp(back_coords.x, 0, gl_NumWorkGroups.x * WRKGRP_SIZE_X - 1);
    back_coords.y = clamp(back_coords.y, 0, gl_NumWorkGroups.y * WRKGRP_SIZE_Y - 1);
    return img_bilinear(velocitymap, back_coords).xy;
}


void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    
    vec4 out_flux = get_flux(pos);
    vec4 vel      = imageLoad(velocitymap, pos);

    // water height
    vec4 terrain = imageLoad(heightmap, pos);
    float d1 = terrain.b;

    // total height difference
    vec4 d_height;
    d_height.x = terrain.w - get_wheight(pos + ivec2(-1, 0)); // left
    d_height.y = terrain.w - get_wheight(pos + ivec2( 1, 0)); // right
    d_height.z = terrain.w - get_wheight(pos + ivec2( 0, 1)); // top
    d_height.w = terrain.w - get_wheight(pos + ivec2( 0,-1)); // bottom

    vec4 in_flux;
    in_flux.x = get_flux(pos + ivec2(-1, 0)).y; // from left
    in_flux.y = get_flux(pos + ivec2( 1, 0)).x; // from right
    in_flux.z = get_flux(pos + ivec2( 0, 1)).w; // from top
    in_flux.w = get_flux(pos + ivec2( 0,-1)).z; // from bottom 

    out_flux.x = 
        max(0, ENERGY_KEPT * out_flux.x + d_t * A * (G * d_height.x) / L);
    out_flux.y =  
        max(0, ENERGY_KEPT * out_flux.y + d_t * A * (G * d_height.y) / L);
    out_flux.z =  
        max(0, ENERGY_KEPT * out_flux.z + d_t * A * (G * d_height.z) / L);
    out_flux.w =
        max(0, ENERGY_KEPT * out_flux.w + d_t * A * (G * d_height.w) / L);

    // boundary checking */
    if (pos.x <= 0) {
        out_flux.x = 0;
    } else if (pos.x >= (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1)) {
        out_flux.y = 0;
    } 
    if (pos.y <= 0) {
        out_flux.w = 0;
    } else if (pos.y >= (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
        out_flux.z = 0;
    } 

    float sum_in_flux = in_flux.x + in_flux.y + in_flux.z + in_flux.w;
    float sum_out_flux = out_flux.x + out_flux.y + out_flux.z + out_flux.w;

     //scaling factor
    float K = min(1.0, (terrain.b * L * L) / (sum_out_flux * d_t));
    out_flux *= K;
    sum_out_flux *= K;
    /* if ((sum_out_flux * d_t) > (L * L * terrain.b)) {
        float K = (terrain.b * L * L) / (sum_out_flux * d_t);
        out_flux *= K;
        sum_out_flux *= K;
    } */
    float d_volume = d_t * (sum_in_flux - sum_out_flux);
    float d2 = max(0, d1 + (d_volume / (L * L)));

    terrain.b = d2;
    terrain.w = terrain.r + d2 + terrain.g;

    /* // velocity advection
    vec2 adv_pos = advect_coords(pos, vel.xy / 2.0, d_t); 
    vec2 adv_vel = get_lerp_vel(adv_pos); */

    // average water height
    vel.z = (d1 + d2); 

    if (vel.z > 0) {
        vel.x = (
            get_flux(pos + ivec2(-1, 0)).y -
            get_flux(pos).x + 
            get_flux(pos).y -
            get_flux(pos + ivec2(1, 0)).x
        ) / (L * vel.z);
        vel.y = (
            get_flux(pos + ivec2(0, -1)).z -
            get_flux(pos).w + 
            get_flux(pos).z -
            get_flux(pos + ivec2(0, 1)).w
        ) / (L * vel.z);
    } else {
        vel.xy = vec2(0, 0);
    }

    imageStore(out_fluxmap, pos, out_flux);
    imageStore(out_velocitymap, pos, vel);
    imageStore(out_heightmap, pos, terrain);
}
