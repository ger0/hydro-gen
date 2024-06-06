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

float find_sin_alpha(ivec2 pos) {
    float r_b = 0.0;
    float l_b = 0.0;
    float d_b = 0.0;
    float u_b = 0.0;

    for (int i = 0; i < SED_LAYERS; i++) {
	    r_b += imageLoad(heightmap, pos + ivec2(1, 0))[i];
	    l_b += imageLoad(heightmap, pos - ivec2(1, 0))[i];
	    d_b += imageLoad(heightmap, pos - ivec2(0, 1))[i];
	    u_b += imageLoad(heightmap, pos + ivec2(0, 1))[i];
	}

	float dbdx = (r_b-l_b) / (2.0 * L);
	float dbdy = (u_b-d_b) / (2.0 * L);

	return sqrt(dbdx*dbdx+dbdy*dbdy)/sqrt(1+dbdx*dbdx+dbdy*dbdy);
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
    if (dd < 1e-04) {
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
    float sin_a = find_sin_alpha(pos);
    for (int i = (SED_LAYERS - 1); i >= 0; i--) {
        // sediment capacity constant for a layer
        // float Klc = Kc * (10 * i + 1);
        float Klc = Kc;
        float Kls = d_t * Ks * (10.0 * i + 1.0);
        float Kld = d_t * Kd * (10.0 * i + 1.0);
        // sediment transport capacity
        float c = max(0.0, Klc * max(0.10, sin_a) * length(vel.xy) - cap);
        // float c = max(0.0, Klc * sin_a * length(vel.xy) - cap);

        float bt;
        float s1;
        float st = sediment[i];

        // dissolve sediment
        if (c > st) {
            bt = terrain[i] - Kls * (c - st);
            s1 = st + Kls * (c - st);
            if (bt < 0.0) {
                float dbt = abs(bt);
                bt = 0;
                s1 -= dbt;
            }
        } 
        // deposit sediment
        else {
            bt = terrain[i] + Kld * (st - c);
            s1 = st - Kld * (st - c);
            if (s1 < 0.0) {
                float ds1 = abs(s1);
                s1 = 0;
                bt -= ds1;
            }
        }
        sediment[i] = s1;
        terrain[i] = bt;
        cap += s1;
        if (bt > 0.0) {
            break;
        }
    }

    terrain.w = terrain.r + terrain.g + terrain.b;
    imageStore(out_velocitymap, pos, vel);
    imageStore(out_sedimap, pos, sediment);
    imageStore(out_heightmap, pos, terrain);
}
