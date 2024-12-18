#define SC (250.0)

vec4 img_bilinear(sampler2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos * WORLD_SCALE);
    vec2 s_pos = fract(sample_pos * WORLD_SCALE);
    vec4 v1 = mix(
        texelFetch(img, ivec2(pos), 0), 
        texelFetch(img, ivec2(pos) + ivec2(1, 0), 0),
        s_pos.x
    );
    vec4 v2 = mix(
        texelFetch(img, ivec2(pos) + ivec2(0, 1), 0), 
        texelFetch(img, ivec2(pos) + ivec2(1, 1), 0),
        s_pos.x
    );
    vec4 value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

