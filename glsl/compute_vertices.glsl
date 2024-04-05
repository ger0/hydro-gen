#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) uniform config {
    ivec2 hmap_dims;
};

uniform mat4 perspective;
uniform mat4 view;

uniform vec3 dir;
uniform vec3 pos;

layout (rgba32f, binding = 2) uniform readonly image2D heightmap;
layout (rgba32f, binding = 3) uniform writeonly image2D out_tex;

const float y_scale     = 48.0; // max height
const float water_lvl   = 18.0; // base water level

const vec3 light_dir    = vec3(0.0, -1.0, -0.21);     

// diffuse colours
const vec3 sky_color    = vec3(0.22, 0.606, 0.964);
const vec3 water_color  = vec3(0.22, 0.606, 0.964);

const vec3 grass_ambient  = vec3(0.014, 0.084, 0.018);
const vec3 rock_ambient   = vec3(0.20,  0.20,  0.20);
const vec3 dirt_ambient   = vec3(0.09,  0.04,  0.02);
const vec3 sand_ambient   = vec3(0.15,  0.15,  0.041);

struct Ray {
    vec3 pos;
    float dist; 
};

// functions
float fog_mix(float fog);
float water_mix(float water);
vec3 get_material_color(vec3 pos, vec3 norm);
vec4 cubic(float v);
float img_bilinear(readonly image2D img, vec2 sample_pos);
vec3 get_img_normal(readonly image2D img, vec2 pos);
float img_bicubic(readonly image2D img, vec2 img_coords);
vec3 get_pixel_color(vec3 origin, vec3 direction);
Ray raymarch(vec3 orig, vec3 dir, const float max_dst, const int max_iter);
vec4 to_world(vec4 coord);

float fog_mix(float fog) {
    return clamp(fog * fog * fog, 0.0, 1.0); 
}

float water_mix(float water) {
    return pow(water, 1.0 / 2.0); 
}

vec3 get_material_color(vec3 pos, vec3 norm) {
    const vec3 up = vec3(0, 1, 0);
    // angle
	float angle = dot(norm, up);

	if (pos.y < (water_lvl + 0.3)) {
	    return sand_ambient;
    } else if (angle < 0.2 || pos.y > (y_scale * 0.75)) {
        return rock_ambient;
	} else if (angle < 0.35 || pos.y > (y_scale * 0.65)) {
	    return dirt_ambient;
	} else {
	    return grass_ambient;
	}
}

vec4 cubic(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;
    return vec4(x, y, z, w) * (1.0 / 6.0);
}

float img_bilinear(readonly image2D img, vec2 sample_pos) {
    ivec2 pos = ivec2(sample_pos);
    vec2 s_pos = fract(sample_pos);
    float v1 = mix(
        imageLoad(img, ivec2(pos)).r, 
        imageLoad(img, ivec2(pos) + ivec2(1, 0)).r,
        s_pos.x
    );
    float v2 = mix(
        imageLoad(img, ivec2(pos) + ivec2(0, 1)).r, 
        imageLoad(img, ivec2(pos) + ivec2(1, 1)).r,
        s_pos.x
    );
    float value = mix(
        v1, 
        v2, 
        s_pos.y
    );
    return value;
}

vec3 get_img_normal(readonly image2D img, vec2 pos) {
    vec3 dpos = vec3(
        -(
            img_bilinear(img, pos - vec2( 1, 0)) - 
            img_bilinear(img, pos - vec2(-1, 0))
        ) * y_scale,
        1.0,
        -(
            img_bilinear(img, pos - vec2(0, 1)) -
            img_bilinear(img, pos - vec2(0,-1))
        ) * y_scale
    );
    return normalize(dpos);
}

float img_bicubic(readonly image2D img, vec2 img_coords) {
    vec2 img_size = imageSize(img);
    vec2 inv_img_size = 1.0 / img_size;

    img_coords = img_coords * img_size - 0.5;

    vec2 fxy = fract(img_coords);
    img_coords -= fxy;

    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);

    vec4 c = img_coords.xxyy + vec2 (-0.5, +1.5).xyxy;

    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;

    offset *= inv_img_size.xxyy;

    float sample0 = img_bilinear(img, offset.xz);
    float sample1 = img_bilinear(img, offset.yz);
    float sample2 = img_bilinear(img, offset.xw);
    float sample3 = img_bilinear(img, offset.yw);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(
        mix(sample3, sample2, sx), 
        mix(sample1, sample0, sx), 
        sy
    );
}

vec3 get_pixel_color(vec3 origin, vec3 direction) {
    const float max_dist = 486.0;
    const int   max_steps = 300;

    Ray ray = raymarch(origin, direction, max_dist, max_steps);
    float fog_buildup = ray.dist / max_dist;

    float water_vol = 0.0;
    // finding the intersection between the ray and the surface of water
    if (origin.y >= water_lvl && ray.pos.y <= water_lvl) {
        const float water_step = 0.1;
        float diff = (water_lvl - ray.pos.y);
        float scale = diff / direction.y;
        water_vol = clamp((length(direction * scale) * water_step), 0.0, 1.0);
    }
    if (ray.dist >= max_dist) {
        // no hit
        vec3 outp = sky_color;
        return mix(
            outp, 
            water_color, 
            water_mix(water_vol)
        );
    }
    // else hit
    vec3 normal = get_img_normal(
        heightmap, 
        ray.pos.xz 
    );
    vec3 ambient = get_material_color(ray.pos.xyz, normal);
    vec3 outp = mix(
        ambient, 
        sky_color, 
        fog_mix(fog_buildup)
    );
    outp = mix(
        outp, 
        water_color, 
        water_mix(water_vol)
    );
    return outp;
}

Ray raymarch(vec3 origin, vec3 direction, 
              const float max_dist, const int max_steps) {
    float dist = 0.001;
    float d_dist = 1.0;

    Ray ret_val;

    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * dist;

        float height = img_bilinear(heightmap, sample_pos.xz) * y_scale;

        // hitting the ground ??
        float d_height = sample_pos.y - height;
        if (d_height <= (0.05)) {
            return Ray(
                sample_pos - (0.05 * direction),
                dist - (0.05)
            );
        }
        if (dist > max_dist) {
            return Ray(sample_pos, max_dist);
        }
        /* dist can't be higher than max slope approximation 
            + accounting for base water level
        */
        d_dist = 0.4 * d_height;
        dist += d_dist;
    }
    return Ray(origin + direction * max_dist, max_dist);
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

    vec2 clip = 2.0 * uv - 1.0;

    vec4 ray_start  = to_world(vec4(0.0, 0.0, -1.0, 1.0));
    vec4 ray_end    = to_world(vec4(clip, 1.0, 1.0));
    vec3 ray_dir = 
        normalize(ray_end.xyz - ray_start.xyz);

    vec3 color = get_pixel_color(pos, ray_dir);
    // gamma correction
    color = pow(color, vec3(1.0/2.2));
    imageStore(out_tex, pixel, vec4(color, 1));
}
