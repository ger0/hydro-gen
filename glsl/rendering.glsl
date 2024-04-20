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

// HEIGHTMAP VARIABLES
const int TERRAIN   = 0x00;
const int SEDIMENT  = 0x01;
const int WATER     = 0x02;
const int TOTAL     = 0x03;

// max raymarching distance
const float max_dist    = 4096.0;
const int   max_steps   = 2048;

const vec3 light_dir    = normalize(vec3(0.0, -0.6, -1.0));
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
// shade individual pixel on the screen
vec3 get_pixel_color(vec3 origin, vec3 direction);
// march rays
Ray raymarch(vec3 orig, vec3 dir, const float max_dst, const int max_iter, const int terr_type);
// convert coords to world coordinates
vec4 to_world(vec4 coord);

float fog_mix(float fog) {
    return clamp(fog * fog * fog, 0.0, 1.0); 
}

float water_mix(float water) {
    return pow(water, 1.0 / 8.0); 
}

vec3 get_material_color(vec3 pos, vec3 norm, Material_colors material) {
    const vec3 up = vec3(0, 1, 0);
    // angle
	float cos_a = dot(norm, -up);
	/* if (angle > 0.6 && pos.y < (water_lvl + 2.3)) {
	    return material.sand; */
    /* if (angle < 0.55 && pos.y > (max_height * 0.45)) {
        return material.rock; */
    if (cos_a < 0.50) {
        return material.rock;
	} else if (cos_a < 0.88) {
	    return material.dirt;
	} else {
	    return material.grass;
	}
	/* } else if (angle > 0.50 && pos.y > (max_height * 0.55)) {
	    return material.snow; */
}

vec3 get_img_normal_w(readonly image2D img, vec2 pos) {
        float dx = (
            img_bilinear_w(img, pos + vec2( 1.0, 0)) - 
            img_bilinear_w(img, pos + vec2(-1.0, 0))
        );
        float dz = (
            img_bilinear_w(img, pos + vec2(0, 1.0)) -
            img_bilinear_w(img, pos + vec2(0,-1.0))
        );
    return normalize(cross(vec3(2.0, dx, 0), vec3(0, dz, 2.0)));
}

vec3 get_img_normal(readonly image2D img, vec2 pos) {
        float dx = (
            img_bilinear_r(img, pos + vec2( 1.0, 0)) - 
            img_bilinear_r(img, pos + vec2(-1.0, 0))
        );
        float dz = (
            img_bilinear_r(img, pos + vec2(0, 1.0)) -
            img_bilinear_r(img, pos + vec2(0,-1.0))
        );
    return normalize(cross(vec3(2.0, dx, 0), vec3(0, dz, 2.0)));
}

vec3 get_fog_color(vec3 col, float ray_dist, float sundot) {
    // fog
    float fo = 1.0 - exp(-pow(0.15 * ray_dist / (max_dist / 16.0), 2.5));
    vec3 fco = 0.65 * vec3(0.4, 0.65, 1.0) + 
        0.1 * vec3(1.0, 0.8, 0.5) * pow(sundot * 1.5, 4.0);
    return mix(col, fco, fo);
}

vec3 get_sky_color(vec3 direction, float ray_dist, float sundot) {
    // sky		
    float diry = direction.y;
    vec3 col = sky_color - diry * diry * 0.37;
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
    Ray shadow = raymarch(ray_pos, -light_dir, shadow_max, max_steps / 2, TERRAIN);

    vec3 ambient;
    float amb_factor = clamp(0.5 + 0.5 * normal.y, 0.0, 1.0);
    // background
    vec3 diffuse;
    if (is_water) {
        //ambient = vec3(0.0035, 0.004, 0.0045);
        ambient = sky_color * 0.005;
        //diffuse = sky_color * 0.1;
        diffuse = vec3(0.108, 0.187, 0.288) * 0.1;

        //float sediment = img_bilinear_g(heightmap, ray_pos.xz);
        //diffuse = mix(diffuse, diffuse_cols.dirt * 100.f, sediment);
    } else {
        ambient = get_material_color(ray_pos.xyz, normal, ambient_cols);
        diffuse = get_material_color(
            ray_pos.xyz, 
            normal, 
            diffuse_cols
        );
    }
    ambient += amb_factor * (diffuse / 16); // bonus
    vec4 outp = vec4(ambient, 0.04);
    // no shadow
    if (shadow.dist >= shadow_max) {
        vec3 light_diff = light_dir;
	    float lambrt = max(dot(light_diff, normal), 0.0);
        outp.rgb += lambrt / 2 * diffuse * shadow.pos.w; 
        outp.w = shadow.pos.w;
    }
    return outp;
}

vec3 get_shade_terr(vec3 ray_pos, vec3 normal) {
    return get_shade(ray_pos, normal, false).xyz;
}

vec3 get_terrain_color(Ray ray, vec3 direction, float sundot) {
    vec3 normal = get_img_normal(
        heightmap, 
        ray.pos.xz 
    );
    vec3 col = get_shade_terr(ray.pos.xyz, normal);
    col = get_fog_color(col, ray.dist, sundot);
    col = mix(
        col,
        get_sky_color(direction, ray.dist, sundot),
        fog_mix(ray.dist / max_dist)
    );
    return col;
}

vec3 refractCameraRay(vec3 surfaceNormal, vec3 cameraDirection, float refractiveIndex) {
    float cosTheta = dot(-surfaceNormal, cameraDirection);
    float sinTheta2 = 1.0 - cosTheta * cosTheta;
    float sinPhi2 = refractiveIndex * refractiveIndex * sinTheta2;

    if (sinPhi2 > 1.0) {
        // Total internal reflection
        return reflect(cameraDirection, surfaceNormal);
    } else {
        float cosPhi = sqrt(1.0 - sinPhi2);
        return refractiveIndex * cameraDirection + (refractiveIndex * cosTheta - cosPhi) * surfaceNormal;
    }
}

vec3 get_water_color(Ray w_ray, vec3 direction, float sundot) {
    // water surface normal
    vec3 w_norm = get_img_normal_w(heightmap, w_ray.pos.xz);

    // cast ray to the bottom of the water
    vec3 refract_dir = refractCameraRay(w_norm, direction, 1.0 / 1.3);
    Ray t_ray = raymarch(
        w_ray.pos.xyz, -refract_dir,
        max_dist / 4.0, max_steps / 4,
        TERRAIN
    );
    float water_vol = t_ray.dist;
    t_ray.dist += w_ray.dist;

    vec3 t_shad = get_terrain_color(t_ray, direction, sundot);

    vec3 w_refl = reflect(direction, w_norm);
    Ray wrefl_ray = raymarch(
        w_ray.pos.xyz, w_refl,
        max_dist, max_steps,
        TERRAIN
    );
    // wrefl_ray.dist += w_ray.dist;

    // get shading for the water surface
    vec4 w_shad = get_shade(w_ray.pos.xyz, w_norm, true);

    vec3 water_col = t_shad;

    // get the color of sky reflected on water
	float refl_sundot = clamp(dot(w_refl, -light_dir), 0.0, 1.0);
    vec3 wrefl_skycol = get_sky_color(
        w_refl,
        wrefl_ray.dist + w_ray.dist,
        refl_sundot
    ); 

    // reflections:
    // fresnel
    float cos_theta = dot(direction, w_norm);
    float fresnel = max(pow(1.0 - cos_theta, 2.0), 0.06);
    // bouncing off to the sky
    if (wrefl_ray.dist >= max_dist) {
        water_col = mix(t_shad, w_shad.rgb, min(1.0, t_ray.dist / 2000000.0));
        water_col = mix(water_col, wrefl_skycol, fresnel);
        water_col = mix(w_shad.rgb, water_col, w_shad.w);
    } 
    // else sampling the reflection of a terrain
    else {
        // get colour of the sampled terrain
        wrefl_ray.dist += w_ray.dist;
        // get the color of terrain from a ray reflected from the surface of water
        water_col = mix(t_shad, w_shad.rgb, min(1.0, t_ray.dist / 2000000.0));
        water_col = mix(
            water_col, 
            get_terrain_color(wrefl_ray, w_refl, refl_sundot),
            w_shad.w
        );
        /* water_col = get_fog_color(
            water_col, 
            wrefl_ray.dist, 
            refl_sundot
        ); */
        water_col = mix(t_shad, water_col, fresnel);
        water_col = mix(
            water_col,
            wrefl_skycol,
            fog_mix(wrefl_ray.dist / (max_dist))
        );
    }
    return water_col;
}

vec3 get_pixel_color(vec3 origin, vec3 direction) {
    Ray ray = raymarch(origin, direction, max_dist, max_steps, TOTAL);
	float sundot = clamp(dot(direction, -light_dir), 0.0, 1.0);

	float water_h = img_bilinear_b(heightmap, ray.pos.xz);

    // no hit - sky
    if (ray.dist >= max_dist) {
        return get_sky_color(direction, ray.dist, sundot);
    }
    // ray hitting the surface of water
    if (water_h > 0.01f) {
        // water buildup
        // float water_vol = 0.0;
        vec3 water_col = get_water_color(ray, direction, sundot);
        water_col = get_fog_color(water_col, ray.dist, sundot);
        water_col = mix(
            water_col,
            get_sky_color(direction, ray.dist, sundot),
            fog_mix(ray.dist / max_dist)
        );
        return water_col;
    } 
    // hit - get terrain
    else {
        return get_terrain_color(ray, direction, sundot);
    }
}

Ray raymarch(
    vec3 origin, vec3 direction, 
    const float max_dist, const int max_steps, 
    const int terr_type
) {
    float dist = 0.001;
    float d_dist = 0.1;

    // shadow penumbra
    float min_sdf = 1.0;
    float t_height = 0.0;

    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * dist;

        if (terr_type == TERRAIN) {
            t_height = img_bilinear_r(heightmap, sample_pos.xz);
        } 
        // ELSE TOTAL
        else {
            t_height = img_bilinear_w(heightmap, sample_pos.xz);
        }

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
