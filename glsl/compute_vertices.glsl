#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (std140, binding = 0) uniform config {
    ivec2   hmap_dims;
};

uniform uint    clip_range;
uniform vec3    sky_color;

uniform mat4 perspective;
uniform mat4 view;

uniform vec3 dir;
uniform vec3 pos;

layout (rgba32f, binding = 2) uniform readonly image2D heightmap;

layout (rgba32f, binding = 3) uniform writeonly image2D out_tex;

vec4 raymarch(vec3 origin, vec3 direction) {
    const int max_steps = 100;
    const float max_distance = 100.0;
    float epsilon = 0.01;

    float total_distance = 0.0;
    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * total_distance;
        float height = imageLoad(heightmap, ivec2(sample_pos.xy)).r * 4.0f;
        if (sample_pos.z < height) {
            // hit
            return vec4(1,1,1,1);
        }
        total_distance += epsilon;
    }
    // no hit
    return vec4(sky_color, 1.0f);
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(gl_NumWorkGroups.xy * gl_WorkGroupSize.xy);

    vec4 ray_start = inverse(perspective * view) * vec4(2.0 * uv - 1.0, -1.0, 1.0);
    vec4 ray_end = inverse(perspective * view) * vec4(2.0 * uv - 1.0, 1.0, 1.0);
    vec3 ray_dir = normalize(ray_end.xyz / ray_end.w - ray_start.xyz / ray_start.w);

    vec4 color = raymarch(ray_start.xyz / ray_start.w, ray_dir);
    /* color = imageLoad(heightmap, pixel);
    color.r *= dir.x;
    color.g *= dir.y;
    color.b *= dir.z; */

    imageStore(out_tex, pixel, color);
}
