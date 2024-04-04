#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) uniform config {
    ivec2 hmap_dims;
    vec4 _sky_color;
    vec4 _water_color;
};

vec4 sky_color      = vec4(0.45, 0.716, 0.914, 1);
vec4 water_color    = vec4(0.15, 0.216, 0.614, 1);

uniform mat4 perspective;
uniform mat4 view;

uniform vec3 dir;
uniform vec3 pos;

layout (rgba32f, binding = 2) uniform readonly image2D heightmap;
layout (rgba32f, binding = 3) uniform writeonly image2D out_tex;

float fog_mix(float fog) {
    return clamp(fog * fog * fog, 0.0, 1.0); 
}

float water_mix(float water) {
    return pow(water, 1.0 / 2.0); 
}

float get_height(vec3 sample_pos) {
    ivec2 pos = ivec2(sample_pos.xz);
    vec2 s_pos = vec2(sample_pos.xz - pos);
    float h1 = mix(
        imageLoad(heightmap, ivec2(pos)), 
        imageLoad(heightmap, ivec2(pos + ivec2(1, 0))),
        s_pos.x
    ).r;
    float h2 = mix(
        imageLoad(heightmap, ivec2(pos) + ivec2(0, 1)), 
        imageLoad(heightmap, ivec2(pos + ivec2(1, 1))),
        s_pos.x
    ).r;
    float height = mix(
        h1, 
        h2, 
        s_pos.y
    ).r;
    return height;
}

vec4 raymarch(vec3 origin, vec3 direction) {
    const float max_dist = 256.0;
    const int   max_steps = 300;

    float blue_bits = 0.0;

    float dist = 0.001;
    float fog_buildup = 0.0;
    float d_dist = 1.0;
    float incr = (1.0 / max_dist) / d_dist;

    float y_scale   = 48.0;
    float water_lvl = 20.0;

    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * dist;
        float height = get_height(sample_pos) * y_scale;

        // water
        if (sample_pos.y <= water_lvl) {
            blue_bits += incr;
            if (blue_bits >= 0.99) break;
        }
        // hitting the ground
        if (sample_pos.y <= height) {
            vec4 outp = mix(
                vec4(0.95, 0.95, 0.95, 1.0), 
                sky_color, 
                fog_mix(fog_buildup)
            );
            outp = mix(
                outp, 
                water_color, 
                water_mix(blue_bits)
            );
            return outp;
        }

        fog_buildup += incr;
        dist += d_dist;
        if (dist > max_dist) break;
    }
    // no hit
    vec4 outp = sky_color;
    outp = mix(
        outp, 
        water_color, 
        water_mix(blue_bits)
    );
    return outp;
}

vec4 to_world(vec4 coord) {
    coord = inverse(perspective) * coord;
    coord /= coord.w;
    coord = inverse(view) * coord;
    // coord /= coord.w;
    return coord;
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pixel) / 
        vec2(gl_NumWorkGroups.xy * gl_WorkGroupSize.xy);

    // vec4 ray_start = vec4(0.0, 0.0, 0.0, 1.0);
    vec2 clip = 2.0 * uv - 1.0;

    vec4 ray_start  = to_world(vec4(0.0, 0.0, -1.0, 1.0));
    vec4 ray_end    = to_world(vec4(clip, 1.0, 1.0));
    vec3 ray_dir = 
        normalize(ray_end.xyz - ray_start.xyz);

    vec4 color = raymarch(pos, ray_dir);
    imageStore(out_tex, pixel, color);
}
