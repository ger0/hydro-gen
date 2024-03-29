#version 460

layout (std430, binding = 0) buffer dims {
    ivec2 dims;
};

// Textures
uniform sampler2D heightmap;  // heightmap texture

// Buffer for storing vertex data (replace with your actual binding point)
buffer VertexBuffer {
    vec4 data[];
} vertices;

void main() {
    // Calculate index into the output vertex buffer based on thread ID
    uint idx = gl_LocalInvocationID.x + gl_LocalInvocationID.y * HEIGHTMAP_WIDTH;

    // Check for out-of-bounds access
    if (idx >= vertices.data.length()) {
        return;
    }

    // Sample height from the heightmap texture
    float height = texture(heightmap, vec2(gl_LocalInvocationID.xy) / vec2(HEIGHTMAP_WIDTH - 1, HEIGHTMAP_HEIGHT - 1)).r;

    // Calculate vertex position based on thread ID and heightmap value
    float x = float(gl_LocalInvocationID.x) / (HEIGHTMAP_WIDTH - 1.0f);
    float y = 0.0f;  // Assuming flat terrain for simplicity, adjust for uneven terrain
    float z = float(gl_LocalInvocationID.y) / (HEIGHTMAP_HEIGHT - 1.0f);
    float worldY = height;  // Scale heightmap value to world space

    // Create vertex data
    vertexData.position = vec4(x, worldY, z, 1.0f);  // 1.0f for w component (homogeneous clip space)

    // Write vertex data to output buffer
    vertices.data[idx] = vertexData.position;
}
