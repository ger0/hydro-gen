#version 460

#include "bindings.glsl"

layout (local_size_x = 8, local_size_y = 8) in;

// (dirt height, rock height, water height, total height)
layout (binding = BIND_HEIGHTMAP, rgba32f)   
	uniform readonly image2D heightmap;
layout (binding = BIND_WRITE_HEIGHTMAP, rgba32f)   
	uniform writeonly image2D out_heightmap;

// (fL, fR, fT, fB) left, right, top, bottom
layout (binding = BIND_FLUXMAP, rgba32f)   
	uniform readonly image2D fluxmap;
layout (binding = BIND_WRITE_FLUXMAP, rgba32f)   
	uniform writeonly image2D out_fluxmap;

// velocity vector
layout (binding = BIND_VELOCITYMAP, rgba32f)   
	uniform readonly image2D velocitymap;
layout (binding = BIND_WRITE_VELOCITYMAP, rgba32f)   
	uniform writeonly image2D out_velocitymap;

// cross-section area of a pipe
const float A = 1.0;
// length of the pipe
const float L = 1.0;
// gravity acceleration
const float G = 9.81;
// time step
const float d_t = 0.01;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    
    vec4 out_flux = imageLoad(fluxmap, pos);
    vec4 vel      = imageLoad(velocitymap, pos);

    // water height
    vec4 terrain = imageLoad(heightmap, pos);
    float d1 = terrain.b;

    // total height difference
    vec4 d_height;
    d_height.x = - imageLoad(heightmap, pos + ivec2(-1, 0)).w + terrain.w; // left
    d_height.y = - imageLoad(heightmap, pos + ivec2( 1, 0)).w + terrain.w; // right
    d_height.z = - imageLoad(heightmap, pos + ivec2( 0, 1)).w + terrain.w; // top
    d_height.w = - imageLoad(heightmap, pos + ivec2( 0,-1)).w + terrain.w; // bottom

    vec4 in_flux;
    in_flux.x = max(0, imageLoad(fluxmap, pos + ivec2(-1, 0)).y); // left
    in_flux.y = max(0, imageLoad(fluxmap, pos + ivec2( 1, 0)).x); // right
    in_flux.z = max(0, imageLoad(fluxmap, pos + ivec2( 0, 1)).w); // top
    in_flux.w = max(0, imageLoad(fluxmap, pos + ivec2( 0,-1)).z); // bottom 

    // left velocity
    out_flux.x = max(0, out_flux.x + d_t * A * (G * d_height.x) / L);
    out_flux.y = max(0, out_flux.y + d_t * A * (G * d_height.y) / L);
    out_flux.z = max(0, out_flux.z + d_t * A * (G * d_height.z) / L);
    out_flux.w = max(0, out_flux.w + d_t * A * (G * d_height.w) / L);

    // scaling factor
    float sum_out_flux = out_flux.x + out_flux.y + out_flux.z + out_flux.w;
    float K = min(1, (d1 * L * L) / (sum_out_flux * d_t));

    out_flux *= K;
    sum_out_flux *= K;

    imageStore(out_fluxmap, pos, out_flux);

    float sum_in_flux = in_flux.x + in_flux.y + in_flux.z + in_flux.w;

    float d_volume = d_t * (sum_in_flux - sum_out_flux);
    float d2 = d1 + (d_volume / (L * L));

    vec4 end_terrain = vec4(terrain.r, 0.0, d2, terrain.r + d2);
    imageStore(out_heightmap, pos, end_terrain);
}
