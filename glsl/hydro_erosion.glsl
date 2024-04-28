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

// velocity + suspended sediment vector
// vec3((u, v), suspended)
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;

uniform float max_height;
float max_flux = max_height * 1000000000;

#define PI 3.1415926538

const float L = 1.0;
// time step
uniform float d_t;
// sediment capacity constant
uniform float Kc;
// sediment dissolving constant
uniform float Ks;
// sediment deposition constant
uniform float Kd;

float find_sin_alpha(ivec2 pos) {
	float r_b = imageLoad(heightmap, pos + ivec2(1, 0)).r;
	float l_b = imageLoad(heightmap, pos - ivec2(1, 0)).r;
	float d_b = imageLoad(heightmap, pos + ivec2(0, 1)).r;
	float u_b = imageLoad(heightmap, pos - ivec2(0, 1)).r;

	float dbdx = (r_b-l_b) / (2.0* L);
	float dbdy = (r_b-l_b) / (2.0* L);

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
    float st = terrain.g;
    // water velocity
    // float dd = clamp(smoothstep(0.01, 6, vel.z - 0.01), 0.01, 6.0);
    float dd = vel.z;
    if (dd < 1e-4) {
        vel.xy = vec2(0,0);
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

    // find normal
    float sin_a = find_sin_alpha(pos);
    float c = Kc * max(0.05, sin_a) * length(vel.xy);
    // float c = Kc * sin_a * length(vel.xy);
    
    /* const float Kdmax = 6.0;
    if (terrain.b >= Kdmax) {
        //c = max(0.0, c - max(0.0, terrain.b - 2.f));
        // deep water doesn't erode as much
        c = 0.0;

    } else {
        c *= (Kdmax - terrain.b) / Kdmax;
    } */

    float bt;
    float s1;

    // dissolve sediment
    if (c > st) {
        bt = terrain.r - d_t * Ks * (c - st);
        s1 = st + d_t * Ks * (c - st);
        // terrain.b = terrain.b + d_t * Ks * (c - st);
    } 
    // deposit sediment
    else {
        bt = terrain.r + d_t * Kd * (st - c);
        s1 = st - d_t * Kd * (st - c);
        // terrain.b = terrain.b - d_t * Kd * (st - c);
    }

    terrain.r = max(0, bt);
    terrain.g = max(0, s1);
    terrain.b = max(0, terrain.b);
    terrain.w = terrain.b + terrain.r;

    imageStore(out_velocitymap, pos, vel);
    imageStore(out_heightmap, pos, terrain);
}
