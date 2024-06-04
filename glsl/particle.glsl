#version 460

#include "bindings.glsl"
#include "img_interpolation.glsl"
#include "simplex_noise.glsl"
#line 7

layout (local_size_x = WRKGRP_SIZE_X + WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)
    uniform readonly image2D heightmap;

layout (std140) uniform map_settings {
    Map_settings_data set;
};

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};

uniform float time;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

float random(uint x) {
    return float(hash(x)) / float(0xFFFFFFFFu);
}

float find_sin_alpha(vec2 pos) {
    float r_b = 0.0;
    float l_b = 0.0;
    float d_b = 0.0;
    float u_b = 0.0;

    for (int i = 0; i < SED_LAYERS; i++) {
	    r_b += img_bilinear(heightmap, pos + vec2(1, 0))[i];
	    l_b += img_bilinear(heightmap, pos - vec2(1, 0))[i];
	    d_b += img_bilinear(heightmap, pos - vec2(0, 1))[i];
	    u_b += img_bilinear(heightmap, pos + vec2(0, 1))[i];
	}

	float dbdx = (r_b-l_b) / (2.0 * L);
	float dbdy = (u_b-d_b) / (2.0 * L);

	return sqrt(dbdx*dbdx+dbdy*dbdy)/sqrt(1+dbdx*dbdx+dbdy*dbdy);
}

vec3 get_terr_normal(vec2 pos) {
    vec2 r = img_bilinear(heightmap, pos + vec2( 1.0, 0)).rg;
    vec2 l = img_bilinear(heightmap, pos + vec2(-1.0, 0)).rg;
    vec2 b = img_bilinear(heightmap, pos + vec2( 0, -1.0)).rg;
    vec2 t = img_bilinear(heightmap, pos + vec2( 0,  1.0)).rg;
    float dx = (
        r.r + r.g - l.r - l.g
    );
    float dz = (
        t.r + t.g - b.r - b.g
    );
    return normalize(cross(vec3(2.0, dx, 0), vec3(0, dz, 2.0)));
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    Particle p = particles[id];
    // spawn particle if there's 0 iteraitons 
    if (p.iters == 0 || p.to_kill == true) {
        vec2 pos = vec2(
            random(uint(id * time * 1000.0)) * float(set.hmap_dims.x) / WORLD_SCALE,
            random(uint((id + 1) * time * 1000.0)) * float(set.hmap_dims.y / WORLD_SCALE)
        );
        p.to_kill = false;
        // p.sediment = vec2(0.0);
        p.position = pos;
        p.velocity = vec2(0);
        p.volume = init_volume;
        p.iters = 0;
    }
    vec3 norm = get_terr_normal(p.position);

    p.velocity = 
        inertia * p.velocity
        - (d_t * norm.xz) / (p.volume * density) * G * (1.0 - inertia);

    // velocity is capped at length 1.0, otherwise particles can tunnel through terrain
    if (length(p.velocity) > 1.0) {
        p.velocity = normalize(p.velocity);
    }

    vec2 old_pos = p.position;
    p.position += d_t * p.velocity;
    if (p.position.x <= 1
        || p.position.y <= 1
        || p.position.x * WORLD_SCALE >= (set.hmap_dims.x - 1.0)
        || p.position.y * WORLD_SCALE >= (set.hmap_dims.y - 1.0)
    ) {
        /* p.velocity = -p.velocity;
        p.position += 2 * d_t * p.velocity * (1 - friction); */
        p.position = old_pos;
        p.to_kill = true;
    }
    p.velocity *= (1.0 - d_t * friction);
    p.volume -= d_t * Ke;

    // sediment transport capacity calculations
    float sin_a = abs(find_sin_alpha(p.position));
    p.sc = max(0.0, Kc * p.volume * length(p.velocity) * max(0.05, sin_a));
    p.iters++;

    if (p.volume <= min_volume 
        || length(p.velocity) < min_velocity
        || p.iters >= ttl
    ) {
        p.to_kill = true;
    }
    particles[id] = p;
}
