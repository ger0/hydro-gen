#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
#line 6

layout (local_size_x = WRKGRP_SIZE_X + WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)
    uniform image2D heightmap;

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};

uniform float time;
// uniform erosion_data

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

void main() {
    uint id = gl_GlobalInvocationID.x;
    vec2 pos = vec2(
        random(uint(id * time * 1000.0)),
        random(uint((id + 1) * time * 1000.0))
    );
    particles[id].position = pos;
}
