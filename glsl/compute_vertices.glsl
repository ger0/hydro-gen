#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (std140, binding = 0) uniform config {
    ivec2       hmap_dims;
    vec3        sky_color;
    unsigned    clip_range;
};

layout (std140, binding = 1) uniform camera {
    mat4 perspective;
    mat4 view;

    vec3 dir;
    vec3 pos;
};

layout (rgba32f, binding = 2) uniform readonly image2D heightmap;

layout (binding = 3, rgba32f) uniform image2D out_tex;

vec4 raymarch(vec3 origin, vec3 direction) {
    const int max_steps = 100;
    const float max_distance = 100.0;
    float epsilon = 0.01;

    float total_distance = 0.0;
    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * total_distance;
        float height = texture(heightmap, sample_pos.xy / hmap_dims).r * 4.0f;
        if (sample_pos.z < height) {
            return vec4(1,1,1,1);
        }
        total_distance += epsilon;
        if (total_distance >= max_distance) {
            // Reached maximum distance without hitting surface, return invalid value
            return vec4(sky_color, 1.0f);
        }
    }
    // Did not hit surface within maximum steps, return invalid value
    return vec4(sky_color, 1.0f);
}

void main() {
    uint idx = gl_LocalInvocationID.x + gl_LocalInvocationID.y * hmap_dims.x;
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(pixel * vec2(2.0)) / vec2(gl_NumWorkGroups.xy * gl_WorkGroupSize.xy);

    vec4 pos = vec4(camera.pos + camera.dir, 1.f) * perspective * view;
    for (unsigned z = 1; z < clip_range; z++) {
        pos 
    }

    float x = float(gl_LocalInvocationID.x) / (hmap_dims.x - 1.0f);
    float height = imageLoad(heightmap, ivec2(gl_LocalInvocationID.xy) / ivec2(hmap_dims.x - 1, hmap_dims.y - 1)).r;
    float z = float(gl_LocalInvocationID.y) / (hmap_dims.y - 1.0f);
    float world_y = height * 4.0f;

    vec4 color = vec4(1, 1, 1, 1);
    ivec2 store_pos = ivec2(gl_GlobalInvocationID.xy);
    imageStore(out_tex, store_pos, color);
}
