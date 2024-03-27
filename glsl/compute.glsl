#version 460 core

layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 0, rgba32f) uniform image2D destTex;

// Function to generate a random float in the range [0, 1]
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);

    // Generate random color values
    vec4 color = vec4(rand(storePos), rand(storePos * 0.5), rand(storePos * 0.25), 1.0);

    // Store the generated color in the output buffer
    imageStore(destTex, storePos, color);
}
