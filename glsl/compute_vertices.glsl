#version 460

layout (local_size_x = 8, local_size_y = 8) in;
layout (std140, binding = 0) uniform config {
    ivec2 hmap_dims;
    uint vert_buff_size;
};
struct Vertex {
    vec4 pos;
    vec4 norm;
};
layout (std140, binding = 1) buffer Vertex_buffer {
    Vertex verts[];
};
layout (rgba32f, binding = 2) uniform readonly image2D heightmap;

void main() {
    uint idx = gl_LocalInvocationID.x + gl_LocalInvocationID.y * hmap_dims.x;

    // Check for out-of-bounds access
    if (idx >= verts.length()) {
        return;
    }

    // Sample height from the heightmap texture
    float height = imageLoad(heightmap, ivec2(gl_LocalInvocationID.xy) / ivec2(hmap_dims.x - 1, hmap_dims.y - 1)).r;

    // Calculate vertex position based on thread ID and heightmap value
    float x = float(gl_LocalInvocationID.x) / (hmap_dims.x - 1.0f);
    float y = 0.0f;  // Assuming flat terrain for simplicity, adjust for uneven terrain
    float z = float(gl_LocalInvocationID.y) / (hmap_dims.y - 1.0f);
    float world_y = height;  // Scale heightmap value to world space

    Vertex vert;
    vert.pos = vec4(x, world_y, z, 1.0f);  // 1.0f for w component (homogeneous clip space)
    // create vert.norm

    verts[idx] = vert;
}
