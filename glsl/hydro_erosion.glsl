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

layout (binding = BIND_SEDIMENTMAP, rgba32f)   
	uniform readonly image2D sedimap;
layout (binding = BIND_WRITE_SEDIMENTMAP, rgba32f)   
	uniform writeonly image2D out_sedimap;

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;

#define PI 3.1415926538

vec3 get_terr_normal(ivec2 pos) {
    vec2 r = imageLoad(heightmap, pos + ivec2( 1, 0)).rg;
    vec2 l = imageLoad(heightmap, pos + ivec2(-1, 0)).rg;
    vec2 b = imageLoad(heightmap, pos + ivec2( 0,-1)).rg;
    vec2 t = imageLoad(heightmap, pos + ivec2( 0, 1)).rg;
    float dx = (
        r.r + r.g - l.r - l.g
    );
    float dz = (
        t.r + t.g - b.r - b.g
    );
    return normalize(cross(vec3(2.0 * L, dx, 0), vec3(0, dz, 2.0 * L)));
}

vec4 get_flux(ivec2 pos) {
    if (pos.x <= 0 || pos.x >= (gl_WorkGroupSize.x * gl_NumWorkGroups.x - 1) ||
    pos.y <= 0 || pos.y >= (gl_WorkGroupSize.y * gl_NumWorkGroups.y - 1)) {
       return vec4(0, 0, 0, 0); 
    }
    return imageLoad(fluxmap, pos);
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 vel = imageLoad(velocitymap, pos);

    vec4 terrain = imageLoad(heightmap, pos);
    vec4 sediment = imageLoad(sedimap, pos);
    // water velocity
    // float dd = clamp(smoothstep(0.01, 6, vel.z - 0.01), 0.01, 6.0);
    float dd = vel.z;
    if (dd < 5e-4) {
        vel.xy = vec2(0, 0);
    } else {
        vel.x = (
            get_flux(pos + ivec2(-1, 0)).y -
            get_flux(pos).x + 
            get_flux(pos).y -
            get_flux(pos + ivec2(1, 0)).x
        ) / (L * dd);
        vel.y = (
            get_flux(pos + ivec2(0, -1)).z -
            get_flux(pos).w + 
            get_flux(pos).z -
            get_flux(pos + ivec2(0, 1)).w
        ) / (L * dd);
    }

    // how much sediment from other layers is already in the water
    float cap = 0.0;
    float n_len = length(get_terr_normal(pos).xz);
    for (int i = (SED_LAYERS - 1); i >= 0; i--) {
        // sediment capacity constant for a layer
        float Kls = d_t * Ks[i];
        float Kld = d_t * Kd[i];
        // sediment transport capacity
        float c = max(0, Kc * max(0.1, n_len) * length(vel.xy) - cap);

        // dissolve sediment
        if (c > sediment[i]) {
            float old_terr = terrain[i];
            terrain[i] -= Kls * (c - sediment[i]);
            sediment[i] += Kls * (c - sediment[i]);
            if (terrain[i] < 0) {
                sediment[i] += terrain[i];
                terrain[i] = 0;
                cap += old_terr;
            } else {
                break;
            }
        } 
        // deposit sediment
        else {
            terrain[i] += Kld * (sediment[i] - c);
            sediment[i] -= Kld * (sediment[i] - c);
        }
    }
    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_velocitymap, pos, vel);
    imageStore(out_sedimap, pos, sediment);
    imageStore(out_heightmap, pos, terrain);
}
