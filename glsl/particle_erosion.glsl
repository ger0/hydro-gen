#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
#line 6

layout (local_size_x = WRKGRP_SIZE_X * WRKGRP_SIZE_Y) in;
layout (binding = BIND_LOCKMAP, r32ui)   
	uniform volatile coherent uimage2D lockmap;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform volatile coherent image2D heightmap;

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};

// erode all layers on 1 point of a quad
void erode_layers(uint id, ivec2 pos, vec2 offset, vec2 old_sediment) {
    Particle part = particles[id];
    vec4 terr = imageLoad(heightmap, pos);
    memoryBarrierImage();
    // weighted multiplier for a point on a quad on which the particle is located
    float multipl = offset.x * offset.y;

    // iterate over all layers
    float cap = 0.0;
    for (int i = (SED_LAYERS - 1); i >= 0; i--) {
        // deposit sediment if the particle is supposed to die
        if (part.to_kill) {
            float sed = old_sediment[i] * multipl;
            terr[i] += sed;
            part.sediment[i] -= sed;
            continue;
        }

        float Kls = d_t * Ks[i];
        float Kld = d_t * Kd[i];

        // sediment transport capacity
        float c = max(0.0, part.sc - cap);

        float s1 = old_sediment[i];
        float old_terr = terr[i];

        // dissolve sediment
        if (c > s1) {
            float eroded = multipl * Kls * (c - s1);
            s1 += eroded;
            terr[i] -= eroded;
            if (terr[i] < 0) {
                s1 += terr[i];
                terr[i] = 0;
                cap += old_terr;
            } else {
                part.sediment[i] = s1;
                break;
            }
        }
        // deposit sediment
        else {
            float deposit = multipl * Kld * (s1 - c);
            s1 -= deposit;
            terr[i] += deposit;
        }
        part.sediment[i] = s1;
    }
    particles[id] = part;
    terr.b += 1e-7 * part.volume * multipl;
    terr.w = terr.r + terr.b + terr.g;
    imageStore(heightmap, pos, terr);
    memoryBarrierImage();
}

// lock a pixel on heightmap and then try to erode the terrain
// spinlock - acquire
void atomic_erosion(uint id, ivec2 pos, vec2 offset, vec2 old_sediment) {
    uint lock_available;
    do {
        lock_available = imageAtomicCompSwap(lockmap, pos, 0, 1);
        if (lock_available == 0) {
            erode_layers(id, pos, offset, old_sediment);
            // release the lock
            imageAtomicExchange(lockmap, pos, 0);
        }
    } while (lock_available != 0);
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    Particle part = particles[id];
    if (part.iters == 0) {
        return;
    }
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
    offset[0] = vec2(1.0 - off.x, 1.0 - off.y);
    offset[1] = vec2(off.x, 1.0 - off.y);
    offset[2] = vec2(off.x, off.y);
    offset[3] = vec2(1.0 - off.x, off.y);
    vec2 sediment = part.sediment;
    barrier();
    for (uint i = 0; i < 4; i++) {
        atomic_erosion(id, pos[i], offset[i], sediment);
    }
}
