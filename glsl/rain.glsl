#version 460

#include "img_interpolation.glsl"
#include "bindings.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform image2D heightmap;

uniform float time;
uniform float max_height;

float rand(float n) {
    return fract(sin(n) * 1e4);
}

#define MOUNT_HGH 0.45

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 terr = imageLoad(heightmap, pos);
    float r = rand(time* sin(pos.x) / WRKGRP_SIZE_X * cos(pos.y) / WRKGRP_SIZE_Y);
    if (r > 0.5) {
        float incr = 0.0001;
        float mountain = terr.w - max_height * MOUNT_HGH;
        if (mountain > 0) {
            incr += mountain * 0.04 / ((1.0 - MOUNT_HGH) * max_height);
        }
        terr.b += incr;
        terr.w = terr.r + terr.b;
    }
    imageStore(heightmap, pos, terr);
}
