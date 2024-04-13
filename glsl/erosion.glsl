#version 460

layout (local_size_x = 8, local_size_y = 8) in;

// (dirt height, rock height, suspended sediment, water height)
layout (binding = 0, rgba32f)   uniform image2D heightmap;
// (fL, fR, fT, fB) left, right, top, bottom
layout (binding = 1, rgba32f)   uniform image2D flux;
// velocity vector
layout (binding = 2, rgba32f)   uniform image2D velocity;

void main() {
}
