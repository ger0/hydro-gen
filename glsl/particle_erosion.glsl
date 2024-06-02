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
void atomic_erosion(uint id, ivec2 pos, vec2 offset) {
    Particle part = particles[id];
    uint lock_available;
    do {
        lock_available = imageAtomicCompSwap(lockmap, pos, 0, 1);
        if (lock_available == 0) {
            vec4 terr = imageLoad(heightmap, pos);
            memoryBarrierImage();
            // weighted multiplier for every 4 points inside a quad
            float multipl = offset.x * offset.y;
            float sed = multipl * d_t * Ks * part.sc;
            part.sediment += sed * part.volume;
            terr.g -= sed * part.volume;
            if (terr.g < 0) {
                float diff = abs(terr.g);
                part.sediment -= diff;
                terr.g += diff;
            }
            terr.w = terr.r + terr.b + terr.g;
            if (part.to_kill = true) {
                terr.g += part.sediment * multipl;
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
    pos[0] = ivec2(part.position * WORLD_SCALE);
    pos[1] = ivec2(part.position * WORLD_SCALE) + ivec2(1, 0);
    pos[2] = ivec2(part.position * WORLD_SCALE) + ivec2(1, 1);
    pos[3] = ivec2(part.position * WORLD_SCALE) + ivec2(0, 1);
    // offset between points inside a quad
    vec2 offset[4];
    vec2 off = fract(part.position * WORLD_SCALE);
    offset[0] = vec2(1 - off.x, 1 - off.y);
    offset[1] = vec2(off.x, 1 - off.y);
    offset[2] = vec2(off.x, off.y);
    offset[3] = vec2(1 - off.x, off.y);
    for (uint i = 0; i < 4; i++) {
        atomic_erosion(id, pos[i], offset[i]);
    }
}
