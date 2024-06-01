#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
#line 6

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
// uniform erosion_data
// particle time to live
const int ttl = 5000;

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

vec4 get_heightmap(vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos);
    vec2 s_pos = fract(sample_pos);
    vec4 v1 = mix(
        imageLoad(heightmap, ivec2(pos)), 
        imageLoad(heightmap, ivec2(pos) + ivec2(1, 0)),
        s_pos.x
    );
    vec4 v2 = mix(
        imageLoad(heightmap, ivec2(pos) + ivec2(0, 1)), 
        imageLoad(heightmap, ivec2(pos) + ivec2(1, 1)),
        s_pos.x
    );
    vec4 value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

vec3 get_terr_normal(vec2 pos) {
    vec2 r = get_heightmap(pos + vec2( 1.0, 0)).rg;
    vec2 l = get_heightmap(pos + vec2(-1.0, 0)).rg;
    vec2 b = get_heightmap(pos + vec2( 0,-1.0)).rg;
    vec2 t = get_heightmap(pos + vec2( 0, 1.0)).rg;
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
    Particle particle = particles[id];
    // spawn particle if there's 0 iteraitons 
    if (particle.iters == 0) {
        vec2 pos = vec2(
            random(uint(id * time * 1000.0)) * float(set.hmap_dims.x),
            random(uint((id + 1) * time * 1000.0)) * float(set.hmap_dims.y)
        );
        // vec2 pos = vec2(0 + id, 64);
        particle.sediment = 0.0;
        particle.position = pos;
    }
    vec3 norm = get_terr_normal(particle.position);
    particle.speed = norm.xz * 0.981;
    particle.position += 0.1 * particle.speed;
    particle.iters++;
    particles[id] = particle;
}
