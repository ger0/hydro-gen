#version 460

#include "img_interpolation.glsl"
#include "bindings.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform image2D heightmap;

uniform float time;
uniform float max_height;
uniform float rain_amount;
uniform float MOUNT_HGH; // 0.0-1.0
uniform float mount_mtp;

float rand(float n) {
    return fract(sin(n) * 1e4);
}


void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 terr = imageLoad(heightmap, pos);
    float r = rand(time * fract(sin(pos.x * 1e2)) * fract(cos(pos.y * 1e4)));

    float incr = rain_amount * r;
    /* if (time < 20.f) {
        incr = 0.1 * r;
    } */
    float mountain = terr.w - max_height * MOUNT_HGH;
    if (mountain > 0) {
        incr += mountain * mount_mtp * r / ((1.0 - MOUNT_HGH) * max_height);
    }
    terr.b += incr;
    terr.w = terr.r + terr.b;
    imageStore(heightmap, pos, terr);
}
