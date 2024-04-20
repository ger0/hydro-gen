#version 460

#include "img_interpolation.glsl"
#include "bindings.glsl"

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
    // water velocity
    float d_Wx = (
        get_flux(pos + ivec2(-1, 0)).y -
        get_flux(pos).x + 
        get_flux(pos).y -
        get_flux(pos + ivec2(1, 0)).x
    ) / 2.0;
    float u = d_Wx / (L * vel.z);

    float d_Wy = (
        get_flux(pos + ivec2(0, -1)).z -
        get_flux(pos).w + 
        get_flux(pos).z -
        get_flux(pos + ivec2(0, 1)).w
    ) / 2.0;
    float v = d_Wy / (L * vel.z);

    float st = imageLoad(heightmap, pos).g;
    vec4 terrain = imageLoad(heightmap, pos);

    float sin_a = find_sin_alpha(pos);
    float c = Kc * (sin_a + 0.15) * max(0.15, length(vec2(u, v)));
    if (terrain.b > 1.0) {
        c = max(0.0, (c + 1.0) - terrain.b);
    } else {
        c *= smoothstep(0.0, 1.0, terrain.b);
    }

    float bt;
    float s1;

    // dissolve sediment
    if (c > st) {
        bt = terrain.r - Ks * (c - st);
        s1 = st + Ks * (c - st);
    } 
    // deposit sediment
    else {
        bt = terrain.r + Kd * (st - c);
        s1 = st - Kd * (st - c);
    }
    vec4 flux = imageLoad(fluxmap, pos);
    vel.x = u;
    vel.y = v;
    vel.w = sin_a;
    imageStore(out_fluxmap, pos, flux);
    imageStore(out_velocitymap, pos, vel);
    terrain.r = bt;
    terrain.g = s1;
    terrain.w = terrain.b + bt;
    imageStore(out_heightmap, pos, terrain);

}
