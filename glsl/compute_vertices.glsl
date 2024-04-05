#version 460

layout (local_size_x = 8, local_size_y = 8) in;

layout (binding = 0) uniform config {
    ivec2 hmap_dims;
    vec3 _sky_color;
    vec3 _water_color;
};

uniform mat4 perspective;
uniform mat4 view;

uniform vec3 dir;
uniform vec3 pos;

layout (rgba32f, binding = 2) uniform readonly image2D heightmap;
layout (rgba32f, binding = 3) uniform writeonly image2D out_tex;


const float y_scale   = 48.0; // max height
const float water_lvl = 18.0; // base water level

// diffuse colours
const vec3 sky_color      = vec3(0.45, 0.716, 0.914);
const vec3 water_color    = vec3(0.15, 0.216, 0.614);

const vec3 grass_color    = vec3(0.014, 0.084, 0.018);
const vec3 rock_color     = vec3(0.20,  0.20,  0.20);
const vec3 dirt_color     = vec3(0.09,  0.04,  0.02);
const vec3 sand_color     = vec3(0.15,  0.15,  0.041);

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
	    return sand_color;
    } else if (angle < 0.2 || pos.y > (y_scale * 0.75)) {
        return rock_color;
	} else if (angle < 0.35 || pos.y > (y_scale * 0.65)) {
	    return dirt_color;
	} else {
	    return grass_color;
	}
}

vec4 cubic(float v){
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

vec3 raymarch(vec3 origin, vec3 direction) {
    const float max_dist = 486.0;
    const int   max_steps = 300;

    float blue_bits = 0.0;

    float dist = 0.001;
    float fog_buildup = 0.0;
    float d_dist = 1.0;
    float incr = (1.0 / max_dist) / d_dist;

    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * dist;
        float height = img_bilinear(heightmap, sample_pos.xz) * y_scale;

        // water
        if (sample_pos.y <= water_lvl) {
            blue_bits += incr;
            if (blue_bits >= 0.99) break;
        }
        // hitting the ground ??
        float d_height = sample_pos.y - height;
        if (d_height <= (0.05 * d_dist)) {
            vec3 normal = get_img_normal(
                heightmap, 
                sample_pos.xz - (0.05 * d_dist * direction.xz)
            );
            vec3 diffuse = get_material_color(sample_pos, normal);
            vec3 outp = mix(
                diffuse, 
                sky_color, 
                fog_mix(fog_buildup)
            );
            outp = mix(
                outp, 
                water_color, 
                water_mix(blue_bits)
            );
            return outp;
        }

        dist += 0.4 * d_height;
        fog_buildup += (0.4 * d_height) / max_dist;
        if (dist > max_dist) break;
    }
    // no hit
    vec3 outp = sky_color;
    outp = mix(
        outp, 
        water_color, 
        water_mix(blue_bits)
    );
    return outp;
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

    vec3 color = raymarch(pos, ray_dir);
    // gamma correction
    color = pow(color, vec3(1.0/2.2));
    imageStore(out_tex, pixel, vec4(color, 1));
}
