#version 460

#include "bindings.glsl"
#include "img_interpolation.glsl"
#include "simplex_noise.glsl"
#line 7

layout (local_size_x = WRKGRP_SIZE_X * WRKGRP_SIZE_Y) in;

layout (binding = 0) uniform sampler2D heightmap;
layout (binding = 1) uniform sampler2D momentmap;

layout (std140) uniform map_settings {
    Map_settings_data map_set;
};

layout (std140, binding = BIND_UNIFORM_EROSION) uniform erosion_data {
    Erosion_data set;
};

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};

uniform float time;
uniform bool should_rain;

/* uint hash(uint x) {
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
*/
float rand(vec2 p) {
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) *
                 (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

vec2 pos_to_uv(vec2 pos) {
    return vec2(pos.x / float(map_set.hmap_dims.x), pos.y / float(map_set.hmap_dims.y));
}

vec2 get_momentum(vec2 pos) {
    return texture(momentmap, pos_to_uv(pos)).xy;
}

vec3 get_terr_normal(vec2 pos) {
    vec2 r = texture(heightmap, pos_to_uv(pos + vec2( 1.0, 0))).rg;
    vec2 l = texture(heightmap, pos_to_uv(pos + vec2(-1.0, 0))).rg;
    vec2 b = texture(heightmap, pos_to_uv(pos + vec2( 0, -1.0))).rg;
    vec2 t = texture(heightmap, pos_to_uv(pos + vec2( 0,  1.0))).rg;
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
    if (p.iters == 0 && should_rain == false) {
        return;
    }
    for (uint i = 0; i < SED_LAYERS; i++) {
        if (p.sediment[i] < 0.0 || p.iters == 0) {
            p.sediment[i] = 0;
        }
    }
    if (p.iters == 0 || p.to_kill == true) {
        vec2 pos = vec2(
            rand(vec2(fract(time * 1.37) * 1000.0, float(id))) * float(map_set.hmap_dims.x - 4.0) / WORLD_SCALE + 2.0,
            rand(vec2(fract(time * 7.21) * 1000.0, float(id) + 3.14)) * float(map_set.hmap_dims.y - 4.0) / WORLD_SCALE + 2.0
        );
        p.to_kill = false;
        p.position = pos;
        p.velocity = vec2(0);
        p.volume = set.init_volume;
        if (should_rain) {
            p.iters = 1;
        } else {
            p.iters = 0;
            return;
        }
    }
    vec3 norm = get_terr_normal(p.position);
    vec2 momentum = get_momentum(p.position);
    float water = texture(heightmap, pos_to_uv(p.position)).b;

    p.velocity -= (set.d_t * norm.xz) / (p.volume) * set.G;

    if(length(momentum) > 0 && length(p.velocity) > 0) {
        p.velocity += set.inertia * dot(normalize(momentum), normalize(p.velocity)) / (p.volume + 1e5 * water) * momentum;
    }

    // velocity is capped at length 1.0, otherwise particles can tunnel through terrain
    if (length(p.velocity) > 1.0) {
        p.velocity = normalize(p.velocity);
    }

    vec2 old_pos = p.position;
    p.position += set.d_t * p.velocity;
    if (p.position.x <= 1
        || p.position.y <= 1
        || p.position.x * WORLD_SCALE >= (map_set.hmap_dims.x - 2)
        || p.position.y * WORLD_SCALE >= (map_set.hmap_dims.y - 2)
    ) {
        /* p.velocity = -p.velocity;
        p.position += 2 * set.d_t * p.velocity * (1 - set.friction); */
        p.position = old_pos;
        p.velocity = vec2(0);
        p.to_kill = true;
    }
    p.velocity *= (1.0 - set.d_t * set.friction * norm.y);
    p.volume -= set.d_t * set.Ke;

    // sediment transport capacity calculations
    float sin_a = length(abs(sqrt(1.0 - norm.y * norm.y)));

    p.sc = max(0.0, set.Kc * p.volume * length(p.velocity) * max(0.02, sin_a));
    p.iters++;

    if (p.volume <= set.min_volume 
        || length(p.velocity) < set.min_velocity
        || p.iters >= set.ttl
    ) {
        p.to_kill = true;
    }
    particles[id] = p;
}
