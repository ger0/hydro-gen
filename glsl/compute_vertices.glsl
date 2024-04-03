#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) uniform config {
    ivec2 hmap_dims;
    vec3 sky_color;
    uint clip_range;
};

uniform mat4 perspective;
uniform mat4 view;

uniform vec3 dir;
uniform vec3 pos;

layout (rgba32f, binding = 2) uniform readonly image2D heightmap;

layout (rgba32f, binding = 3) uniform writeonly image2D out_tex;

vec4 raymarch(vec3 origin, vec3 direction) {
    const int max_steps = 100;
    const float max_distance = clip_range;
    float epsilon = 1.0;

    float total_distance = 0.0;
    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * total_distance;
        float height = imageLoad(heightmap, ivec2(sample_pos.xz)).r * 20.f;
        if (sample_pos.y < height) {
            // hit
            return mix(vec4(1,1,1,1), vec4(sky_color,1), total_distance);
        }
        total_distance += epsilon;
    }
    // no hit
    return vec4(sky_color, 1.0f);
}

vec4 to_world(vec4 coord) {
    coord = inverse(perspective) * coord;
    coord /= coord.w;
    coord = inverse(view) * coord;
    coord /= coord.w;
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
    // vec4 color = vec4(vec3(ray_dir), 1.f);
    imageStore(out_tex, pixel, color);
}
