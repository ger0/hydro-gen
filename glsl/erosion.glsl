#version 460

layout (local_size_x = 8, local_size_y = 8) in;

// (dirt height, rock height, water height, total height)
layout (binding = 0, rgba32f)   uniform readonly image2D in_heightmap;
layout (binding = 1, rgba32f)   uniform writeonly image2D out_heightmap;
// (fL, fR, fT, fB) left, right, top, bottom
layout (binding = 2, rgba32f)   uniform image2D flux;
// velocity vector
layout (binding = 3, rgba32f)   uniform image2D velocity;

void main() {
}
