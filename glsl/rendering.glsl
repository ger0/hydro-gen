#version 460

#include "bindings.glsl"
#include "img_interpolation.glsl"

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (std140) uniform config {
    float max_height;
    ivec2 hmap_dims;
};

uniform mat4 perspective;
uniform mat4 view;
uniform vec3 dir;
uniform vec3 pos;
uniform float time;
uniform bool should_draw_water;

layout (rgba32f, binding = BIND_HEIGHTMAP) 
	uniform readonly image2D heightmap;

layout (rgba32f, binding = BIND_DISPLAY_TEXTURE) 
	uniform writeonly image2D out_tex;

// max raymarching distance
const float max_dist    = 8096.0;
const int   max_steps   = 4096;

const vec3 light_dir    = normalize(vec3(0.0, -1.0, -0.5));
const vec3 light_color  = normalize(vec3(0.09, 0.075, 0.04));

// diffuse colours
const vec3 sky_color    = vec3(0.3, 0.5, 0.85);
const vec3 water_color  = vec3(0.22, 0.606, 0.964);

struct Material_colors {
    vec3 grass;
    vec3 rock;
    vec3 dirt;
    vec3 sand;
    vec3 snow;
};

// ambient colors
const Material_colors ambient_cols = Material_colors(
    vec3(0.00014, 0.00084, 0.00018),
    vec3(0.0002,  0.0002,  0.0002),
    vec3(0.0009,  0.0004,  0.0002),
    vec3(0.0015,  0.0015,  0.00041),
    vec3(0.004, 0.0038, 0.0039)
);

// diffuse colors
const Material_colors diffuse_cols = Material_colors(
    //vec3(0.014, 0.074, 0.009) / 1.5,
    vec3(0.2068, 0.279, 0.01) * 1.3 / 5,
    vec3(0.30,  0.29,  0.17) * 1.3 / 6,
    //vec3(0.07,  0.04,  0.02) / 2.0,
    vec3(0.2068, 0.179, 0.00) * 1.3 / 6,
    vec3(0.15,  0.15,  0.041) / 2,
    vec3(0.5, 0.48, 0.49) * 2 / 3
);

struct Ray {
    vec4 pos;
    float dist; 
};

// functions
// a different curve between [0 - 1] values
float fog_mix(float fog);
float water_mix(float water);
// returns a color depending on the heightmap
vec3 get_material_color(vec3 pos, vec3 norm, Material_colors material);
// get a interpolated normal based on image2D
vec3 get_img_normal(readonly image2D img, vec2 pos);
// retrieve color from the material hit by the ray
vec4 get_shade(vec3 ray_pos, vec3 normal, bool is_water);
// retrieve color from the terrain hit by the ray 
vec3 get_shade_terr(vec3 ray_pos, vec3 normal);
// returns the color of the fog based on camera and sun direction
vec3 get_fog_color(vec3 col, float ray_dist, float sundot);
// returns the color of the sky based on camera and sun direction
vec3 get_sky_color(vec3 direction, float ray_dist, float sundot);
// get height of the top and bottom of the water body
vec2 get_water_top_bottom(vec3 pos);
// shade individual pixel on the screen
vec3 get_pixel_color(vec3 origin, vec3 direction);
// march rays
Ray raymarch(vec3 orig, vec3 dir, const float max_dst, const int max_iter);
// convert coords to world coordinates
vec4 to_world(vec4 coord);

vec2 get_water_top_bottom(vec3 pos) {
    float dirt = img_bilinear_r(heightmap, pos.xz);
    float water = img_bilinear_b(heightmap, pos.xz);
    return vec2(dirt + water, dirt);
}

float fog_mix(float fog) {
    return clamp(fog * fog * fog, 0.0, 1.0); 
}

float water_mix(float water) {
    return pow(water, 1.0 / 8.0); 
}

vec3 get_material_color(vec3 pos, vec3 norm, Material_colors material) {
    const vec3 up = vec3(0, 1, 0);
    // angle
	float angle = dot(norm, up);
	/* if (angle > 0.6 && pos.y < (water_lvl + 2.3)) {
	    return material.sand; */
    /* if (angle < 0.55 && pos.y > (max_height * 0.45)) {
        return material.rock; */
    /* } else if (angle < 0.45) {
        return material.rock; */
	if (angle < 0.85) {
	    return material.dirt;
	} else {
	    return material.grass;
	}
	/* } else if (angle > 0.50 && pos.y > (max_height * 0.55)) {
	    return material.snow; */
}

vec3 get_img_normal_w(readonly image2D img, vec2 pos) {
    vec3 dpos = vec3(
        (
            img_bilinear_w(img, pos + vec2( 1, 0)) - 
            img_bilinear_w(img, pos + vec2(-1, 0))
        ),
        1.0,
        (
            img_bilinear_w(img, pos + vec2(0, 1)) -
            img_bilinear_w(img, pos + vec2(0,-1))
        )
    );
    return normalize(dpos);
}

vec3 get_img_normal(readonly image2D img, vec2 pos) {
    vec3 dpos = vec3(
        (
            img_bilinear_r(img, pos + vec2( 1, 0)) - 
            img_bilinear_r(img, pos + vec2(-1, 0))
        ),
        1.0,
        (
            img_bilinear_r(img, pos + vec2(0, 1)) -
            img_bilinear_r(img, pos + vec2(0,-1))
        )
    );
    return normalize(dpos);
}

vec3 get_fog_color(vec3 col, float ray_dist, float sundot) {
    // fog
    float fo = 1.0 - exp(-pow(0.01 * ray_dist / SC, 1.5));
    vec3 fco = 0.65 * vec3(0.4, 0.65, 1.0) + 
        0.1 * vec3(1.0, 0.8, 0.5) * pow(sundot, 2.0);
    return mix(col, fco, fo);
}

vec3 get_sky_color(vec3 direction, float ray_dist, float sundot) {
    // sky		
    float diry = min(direction.y, 0.78);
    vec3 col = sky_color - diry * diry * 0.5;
    col = mix(
        col,
        0.85 * vec3(0.7,0.75,0.85),
        pow(1.0 - max(diry, 0.0), 4.0)
    );
    // sun
    col += 0.25 * vec3(1.0,0.7,0.4) * pow(sundot, 5.0);
    col += 0.25 * vec3(1.0,0.8,0.6) * pow(sundot, 64.0);
    col += 0.2  * vec3(1.0,0.8,0.6) * pow(sundot, 512.0);
    /* // clouds
	vec2 sc = origin.xz + direction.xz*(SC*1000.0-origin.y)/diry;
	col = mix( col, vec3(1.0,0.95,1.0), 0.5*smoothstep(0.5,0.8,fbm(0.0005*sc/SC)) ); */
    // horizon
    return mix(
        col, 
        0.68 * vec3(0.4, 0.65, 1.0), 
        pow(1.0 - max(diry, 0.0), 16.0)
    );
}

vec4 get_shade(vec3 ray_pos, vec3 normal, bool is_water) {
    // shadow 
    // difference to the point above which there's no possible surface
    float shadow_diff = max_height - ray_pos.y;
    float shadow_scale = shadow_diff / -light_dir.y;
    float shadow_max = length(light_dir * shadow_scale);
    // casting a shadow ray towards the sun
    Ray shadow = raymarch(ray_pos, -light_dir, shadow_max, max_steps / 2);

    vec3 ambient;
    float amb_factor = clamp(0.5 + 0.5 * normal.y, 0.0, 1.0);
    // background
    if (is_water) {
        ambient = vec3(0.0035, 0.004, 0.0045);
    } else {
        ambient = get_material_color(ray_pos.xyz, normal, ambient_cols);
    }
    vec3 diffuse = get_material_color(
        ray_pos.xyz, 
        normal, 
        diffuse_cols
    );
    ambient += amb_factor * (diffuse / 16); // bonus
    vec4 outp = vec4(ambient, 0.04);
    // no shadow
    if (shadow.dist == shadow_max) {
        vec3 light_diff = light_dir;
        light_diff.y = -light_diff.y;
	    float lambrt = max(dot(light_diff, normal), 0.0);
        outp.rgb += lambrt / 2 * diffuse * shadow.pos.w; 
        outp.w = shadow.pos.w;
    }
    return outp;
}

vec3 get_shade_terr(vec3 ray_pos, vec3 normal) {
    return get_shade(ray_pos, normal, false).xyz;
}

vec4 calc_water(vec3 in_color, Ray ray, vec3 direction, float sundot, vec2 water_lvls) {
    // debugging TODO: REMOVE
    //return vec4(0, 0, 1, 0.6);

    const float water_step = 0.01;
    float diff = (water_lvls.r - ray.pos.y);
    float scale = diff / direction.y;
    float water_vol = clamp((length(direction * scale) * water_step), 0.0, 1.0);

    // the point where the ray hit the water surface
    vec3 w_orig = ray.pos.xyz + (direction * scale);
    float rdist_to_w = ray.dist - length(direction * scale);

    // a ray out of water's surface
    vec3 w_norm = get_img_normal_w(heightmap, w_orig.xz);

    w_norm = normalize(w_norm);
    vec3 w_refl = reflect(direction, -w_norm);
    Ray w_ray = raymarch(w_orig, w_refl, max_dist / 2.0, max_steps / 2);

    // shadow
    vec4 w_shad = get_shade(w_orig, w_norm, true);
    vec3 water_col = w_shad.rgb;
    // get the color of sky reflected on water
    vec3 col = get_sky_color(w_refl, ray.dist, sundot); 

    // fresnel
    float cos_theta = dot(normalize(-direction), w_norm);
    float fresnel = max(pow(1.0 - cos_theta, 2.0), 0.17);

    // bouncing off to the sky
    if (w_ray.dist >= (max_dist / 4.0)) {
        water_col = col;
        water_col = mix(in_color, water_col, fresnel);
        water_col = mix(w_shad.rgb, water_col, w_shad.w);
    } else {
        vec3 t_norm = get_img_normal(heightmap, w_ray.pos.xz);
        // get shadow of the sampled terrain
        water_col = mix(
            w_shad.rgb, 
            get_shade_terr(w_ray.pos.xyz, t_norm), 
            w_shad.w
        );
        water_col = mix(in_color, water_col, fresnel);
        water_col = get_fog_color(
            water_col, 
            w_ray.dist + rdist_to_w, 
            sundot
        );
        water_col = mix(
            water_col,
            col,
            fog_mix((w_ray.dist + rdist_to_w) / max_dist)
        );
    }
    return vec4(water_col, water_vol);
}

vec3 get_pixel_color(vec3 origin, vec3 direction) {
    Ray ray = raymarch(origin, direction, max_dist, max_steps);
	float sundot = clamp(dot(direction, -light_dir), 0.0, 1.0);

    // top, bottom heights of water body
    vec2 water_lvls = get_water_top_bottom(ray.pos.xyz);

    // water buildup
    float water_vol = 0.0;
    vec3 water_col = vec3(0,0,0);

    // no hit - sky
    if (ray.dist >= max_dist) {
        vec3 col = get_sky_color(direction, ray.dist, sundot);
        return col;
    }
    // hit - get terrain
    vec3 normal = get_img_normal(
        heightmap, 
        ray.pos.xz 
    );
    vec3 outp = get_shade_terr(ray.pos.xyz, normal);
    // if the hit is below water level
    // find the intersection of the ray and the surface of water
    if (ray.pos.y <= water_lvls.r) {
        vec4 res = calc_water(outp, ray, direction, sundot, water_lvls);
        water_col = res.rgb;
        water_vol = res.w;
    }
    outp = mix(
        outp, 
        water_col, 
        water_mix(water_vol)
    );
    outp = get_fog_color(outp, ray.dist, sundot);
    outp = mix(
        outp,
        get_sky_color(direction, ray.dist, sundot),
        fog_mix(ray.dist / max_dist)
    );
    return outp;
}

Ray raymarch(vec3 origin, vec3 direction, 
              const float max_dist, const int max_steps) {
    float dist = 0.001;
    float d_dist = 0.1;

    // shadow penumbra
    float min_sdf = 1.0;

    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * dist;

        // float height = img_bilinear(heightmap, sample_pos.xz);
        float t_height = img_bilinear_r(heightmap, sample_pos.xz);

        if (sample_pos.y > max_height && direction.y >= 0) {
            break;
        }
        // hitting the ground ??
        float d_height = sample_pos.y - t_height;
        if (abs(d_height) <= 0.05) {
            return Ray(
                vec4(sample_pos - (0.05 * direction), 0.0),
                dist - (0.05)
            );
        }
        // penumbra
        min_sdf = min(min_sdf, 12 * d_dist / dist);
        if (dist > max_dist) {
            return Ray(
                vec4(sample_pos, min_sdf),
                max_dist
            );
        }
        // TODO: dist can't be higher than max slope approximation 
        d_dist = 0.35 * d_height;
        dist += d_dist;
    }
    return Ray(
        vec4(origin + direction * max_dist, min_sdf),
        max_dist
    );
}

vec4 to_world(vec4 coord) {
    coord = inverse(perspective) * coord;
    coord /= coord.w;
    coord = inverse(view) * coord;
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
    color = pow(color, vec3(1.0 / 2.2));
    imageStore(out_tex, pixel, vec4(color, 0));
}
