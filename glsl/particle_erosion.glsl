#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
#line 6

layout (local_size_x = WRKGRP_SIZE_X + WRKGRP_SIZE_Y) in;
layout (binding = BIND_LOCKMAP, r32ui)   
	uniform volatile coherent uimage2D lockmap;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform volatile coherent image2D heightmap;

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};


// spinlock - acquire
void atomic_erosion(uint id, ivec2 pos, vec2 sh) {
    Particle part = particles[id];
    uint lock_available;
    do {
        lock_available = imageAtomicCompSwap(lockmap, pos, 0, 1);
        if (lock_available == 0) {
            vec4 terr = imageLoad(heightmap, pos);
            memoryBarrierImage();
            part.sediment += d_t * Ks * part.sc;
            terr.g -= d_t * Ks * part.volume * part.sc;
            if (part.to_kill = true) {
                terr.g += part.sediment;
            }
            imageStore(heightmap, pos, terr);
            memoryBarrierImage();

            // release the lock
            imageAtomicExchange(lockmap, pos, 0);
        }
    } while (lock_available != 0);
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    Particle part = particles[id];
    // get a quad
    ivec2 pos[4];
    //  3---2
    //  |   |
    //  0---1
    pos[0] = ivec2(part.position);
    pos[1] = ivec2(part.position) + ivec2(1, 0);
    pos[2] = ivec2(part.position) + ivec2(1, 1);
    pos[3] = ivec2(part.position) + ivec2(0, 1);
    // offset between points inside a quad
    vec2 sh = fract(part.position);
    for (uint i = 0; i < 4; i++) {
        atomic_erosion(id, pos[i], sh);
    }
}
