#version 460

#include "bindings.glsl"
#include "simplex_noise.glsl"
#line 6

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform image2D heightmap;

uniform float time;
layout (std140, binding = BIND_UNIFORM_RAIN_SETTINGS) 
uniform settings {
    Rain_data set;
};

layout (std140, binding = BIND_UNIFORM_MAP_SETTINGS)
uniform map_settings {
    Map_settings_data map_set;
};

float rand(float n) {
    return fract(sin(n) * 1e4);
}

float hash(vec2 p) {
    p  = 50.0 * fract( p*0.3183099 );
    return fract(p.x*p.y*(p.x+p.y));
}

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec4 terr = imageLoad(heightmap, pos);
    //float r = rand(time * fract(sin(pos.x * 1e2)) * fract(cos(pos.y * 1e4)));
    gln_tFBMOpts opts = gln_tFBMOpts(
        fract(time * 1.372914227e3) * 1000.f,
        0.5,
        2.0,
        set.drops,
        1,
        8,
        false,
        false
    );
    float r = max(0.0, gln_sfbm(gl_GlobalInvocationID.xy, opts));

    float incr = set.amount * r;
    float mountain = terr.w - map_set.max_height * set.mountain_thresh;
    if (mountain > 0) {
        incr += mountain * set.mountain_multip * r / ((1.0 - set.mountain_thresh) * map_set.max_height);
    }
    terr.b += incr;
    terr.w = terr.r + terr.b;
    imageStore(heightmap, pos, terr);
}
