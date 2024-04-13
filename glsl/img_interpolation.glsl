#define SC (250.0)
#define WORLD_SCALE (2.0)

vec4 img_bilinear(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos * WORLD_SCALE);
    vec2 s_pos = fract(sample_pos * WORLD_SCALE);
    vec4 v1 = mix(
        imageLoad(img, ivec2(pos)), 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)),
        s_pos.x
    );
    vec4 v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)), 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)),
        s_pos.x
    );
    vec4 value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

float img_bilinear_w(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos * WORLD_SCALE);
    vec2 s_pos = fract(sample_pos * WORLD_SCALE);
    float v1 = mix(
        imageLoad(img, ivec2(pos)).w, 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)).w,
        s_pos.x
    );
    float v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)).w, 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)).w,
        s_pos.x
    );
    float value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

float img_bilinear_b(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos * WORLD_SCALE);
    vec2 s_pos = fract(sample_pos * WORLD_SCALE);
    float v1 = mix(
        imageLoad(img, ivec2(pos)).b, 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)).b,
        s_pos.x
    );
    float v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)).b, 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)).b,
        s_pos.x
    );
    float value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

float img_bilinear_g(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos * WORLD_SCALE);
    vec2 s_pos = fract(sample_pos * WORLD_SCALE);
    float v1 = mix(
        imageLoad(img, ivec2(pos)).g, 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)).g,
        s_pos.x
    );
    float v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)).g, 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)).g,
        s_pos.x
    );
    float value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

float img_bilinear_r(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos * WORLD_SCALE);
    vec2 s_pos = fract(sample_pos * WORLD_SCALE);
    float v1 = mix(
        imageLoad(img, ivec2(pos)).r, 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)).r,
        s_pos.x
    );
    float v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)).r, 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)).r,
        s_pos.x
    );
    float value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

// unused 
vec4 cubic(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0 / 6.0);
}

float img_bicubic_r(readonly image2D img, vec2 img_coords) {
    vec2 img_size = imageSize(img);
    vec2 inv_img_size = 1.0 / img_size;

    img_coords = img_coords * img_size - 0.5;

    vec2 fxy = fract(img_coords);
    img_coords -= fxy;

    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);

    vec4 c = img_coords.xxyy + vec2 (-0.5, +1.5).xyxy;

    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;

    offset *= inv_img_size.xxyy;

    float sample0 = img_bilinear_r(img, offset.xz);
    float sample1 = img_bilinear_r(img, offset.yz);
    float sample2 = img_bilinear_r(img, offset.xw);
    float sample3 = img_bilinear_r(img, offset.yw);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(
        mix(sample3, sample2, sx), 
        mix(sample1, sample0, sx), 
        sy
    );
}

