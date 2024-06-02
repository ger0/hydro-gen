#version 460

#include "bindings.glsl"
#include "img_interpolation.glsl"
#line 6

layout (local_size_x = WRKGRP_SIZE_X, local_size_y = WRKGRP_SIZE_Y) in;

layout (std140, binding = BIND_UNIFORM_MAP_SETTINGS)
uniform map_settings {
    Map_settings_data set;
};

layout(std430, binding = BIND_PARTICLE_BUFFER) buffer ParticleBuffer {
    Particle particles[];
};

// TODO: Move this out to a special buffer
uniform mat4 perspective;
uniform mat4 view;
uniform vec3 dir;
uniform vec3 pos;
uniform float time;
uniform bool should_draw_water;
uniform bool DEBUG_PREVIEW;

uniform float prec = 0.35;

uniform bool  display_sediment = false;
uniform float  sediment_max_cap = 0.1;

layout (rgba32f, binding = BIND_HEIGHTMAP) 
	uniform readonly image2D heightmap;

layout (rgba32f, binding = BIND_SEDIMENTMAP) 
	uniform readonly image2D sedimentmap;

layout (rgba32f, binding = BIND_DISPLAY_TEXTURE) 
	uniform writeonly image2D out_tex;

// HEIGHTMAP VARIABLES
const int ROCK      = 0x00;
const int DIRT      = 0x01;
const int WATER     = 0x02;
const int TOTAL     = 0x03;
const int TERRAIN   = 0x04;
const int SEDIMENT  = 0x05;

// max raymarching distance
const float max_dist    = 2048.0;
const int   max_steps   = 1024;

const vec3 light_dir    = normalize(vec3(0.0, -0.7, 1.0));
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
    vec3(0.0008,  0.0008,  0.0008),
    vec3(0.00032,  0.00030,  0.0018),
    vec3(0.0015,  0.0015,  0.00041),
    vec3(0.004, 0.0038, 0.0039)
);

// diffuse colors
const Material_colors diffuse_cols = Material_colors(
    vec3(0.2068, 0.279, 0.01) * 1.3 / 5,
    vec3(0.40,  0.39,  0.37) * 1.3 / 6,
    vec3(0.2068, 0.179, 0.00) * 1.3 / 6,
    vec3(0.25,  0.25,  0.081) / 2,
    vec3(0.5, 0.48, 0.49) * 2 / 3
);

struct Ray {
    vec4 pos;
    float dist; 
    vec4 terr;
};

// functions
// a different curve between [0 - 1] values
float fog_mix(float fog);
float water_mix(float water);
// returns a color depending on the heightmap
vec3 get_material_color(Ray ray, vec3 norm, Material_colors material);
// get a interpolated normal based on image2D
vec3 get_img_normal(readonly image2D img, vec2 pos);
// retrieve color from the material hit by the ray
vec4 get_shade(Ray ray, vec3 normal, bool is_water);
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

// TODO: remove
float fog_mix(float fog) {
    return clamp(fog * fog, 0.0, 1.0); 
}

float water_mix(float water) {
    return clamp((water - 1e-4) * 100, 0, 1); 
}

vec3 get_material_color(Ray ray, vec3 norm, Material_colors material) {
    const vec3 up = vec3(0, 1, 0);
    // angle
	float cos_a = dot(norm, -up);

	float rock = smoothstep(0.0, 1.0, min(1.0, ray.terr.r));
	float dirt = smoothstep(0.0, 1.0, min(1.0, ray.terr.g));

	vec4 sediment = img_bilinear(sedimentmap, ray.pos.xz);
	float sed_rock = clamp(sediment.r / sediment_max_cap, 0.0, 1.0);
	float sed_dirt = clamp(sediment.g / sediment_max_cap, 0.0, 1.0);
    
    vec3 col;
	col = material.rock;
	if (dirt > 0.0) {
	    if (cos_a < 0.75) {
            float wet_dirt_cos = 1 - smoothstep(0.25, 0.75, cos_a);
	        col = mix(material.dirt, material.dirt * 0.4, wet_dirt_cos);
	    }
	    else if (cos_a < 0.95) {
            float dirt_cos = 1 - smoothstep(0.85, 0.95, cos_a);
	        col = mix(material.grass, material.dirt, dirt_cos);
	    } else {
	        col = material.grass;
	    }
	    col = mix(material.rock, col, dirt);
    } else {
        float steep_rock = 1 - smoothstep(0.00, 0.65, cos_a);
        col = mix(material.rock, material.rock * 0.5, steep_rock);
    }
    col = mix(col, material.sand, sed_rock);
    col = mix(col, material.dirt * 0.4, sed_dirt);
	return col;
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

// terrain normal
vec3 get_img_normal(readonly image2D img, vec2 pos) {
    vec2 r = img_bilinear(img, pos + vec2( 1.0, 0)).rg;
    vec2 l = img_bilinear(img, pos + vec2(-1.0, 0)).rg;
    vec2 b = img_bilinear(img, pos + vec2( 0,-1.0)).rg;
    vec2 t = img_bilinear(img, pos + vec2( 0, 1.0)).rg;
    float dx = (
        r.r + r.g - l.r - l.g
    );
    float dz = (
        t.r + t.g - b.r - b.g
    );
    return normalize(cross(vec3(2.0, dx, 0), vec3(0, dz, 2.0)));
}

vec3 get_fog_color(vec3 col, float ray_dist, float sundot) {
    // fog
    float fo = 1.0 - exp(-pow(0.15 * ray_dist / (max_dist), 2.5));
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

vec4 get_shade(Ray ray, vec3 normal, bool is_water) {
    // shadow 
    // difference to the point above which there's no possible surface
    float shadow_diff = set.max_height - ray.pos.y;
    float shadow_scale = shadow_diff / -light_dir.y;
    float shadow_max = length(light_dir * shadow_scale);
    // casting a shadow ray towards the sun
    Ray shadow = raymarch(ray.pos.xyz, -light_dir, shadow_max, max_steps / 2, TERRAIN);

    vec3 ambient;
    float amb_factor = clamp(0.5 + 0.5 * normal.y, 0.0, 1.0);
    // background
    vec3 diffuse;
    if (is_water) {
        //ambient = vec3(0.0035, 0.004, 0.0045);
        ambient = sky_color * 0.005;
        //diffuse = sky_color * 0.1;
        diffuse = vec3(0.108, 0.187, 0.288) * 0.1;
    } else {
        ambient = get_material_color(ray, normal, ambient_cols);
        diffuse = get_material_color(
            ray, 
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

vec3 get_shade_terr(Ray ray, vec3 normal) {
    return get_shade(ray, normal, false).xyz;
}

vec3 get_terrain_color(Ray ray, vec3 direction, float sundot) {
    vec3 normal = get_img_normal(
        heightmap, 
        ray.pos.xz 
    );
    vec3 col = get_shade_terr(ray, normal);
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
        max_dist / 8.0, max_steps / 4,
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
    vec4 w_shad = get_shade(w_ray, w_norm, true);
    vec3 water_col = w_shad.rgb;

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
        //water_col = mix(w_shad.rgb, diffuse_cols.dirt, a);
        water_col = mix(t_shad, water_col, min(1.0, water_vol / 2.0));
        water_col = mix(water_col, wrefl_skycol, fresnel);
        water_col = mix(w_shad.rgb, water_col, w_shad.w);
        // water_col = mix(t_shad, water_col, water_mix(water_vol));
    } 
    // else sampling the reflection of a terrain
    else {
        // get colour of the sampled terrain
        wrefl_ray.dist += w_ray.dist;
        // water_col = mix(w_shad.rgb, diffuse_cols.dirt, a);
        // get the color of terrain from a ray reflected from the surface of water
        water_col = mix(t_shad, water_col, min(1.0, water_vol / 2.0));
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
        // water_col = mix(t_shad, water_col, water_mix(water_vol));
    }
    return water_col;
}

vec3 get_pixel_color(vec3 origin, vec3 direction) {
    Ray ray = raymarch(origin, direction, max_dist, max_steps, TOTAL);
	float sundot = clamp(dot(direction, -light_dir), 0.0, 1.0);

	vec2 pos = ray.pos.xz * WORLD_SCALE;
	// float water_h = img_bilinear_b(heightmap, ray.pos.xz);

	float water_h = ray.terr.b;
    water_h = min(1.0, smoothstep(1e-4, 1.0, water_h));

	if (pos.x < 0 || pos.x >= float(imageSize(heightmap).x) ||
	pos.y < 0 || pos.y >= float(imageSize(heightmap).y)) {
	    water_h = max_dist;
	}

	vec3 color;

    // no hit - sky
    if (ray.dist >= max_dist) {
        color = get_sky_color(direction, ray.dist, sundot);
    }
    // ray hitting the surface of water
    else if (water_h > 1e-12) {
        // water buildup
        vec3 water_col = get_water_color(ray, direction, sundot);
        water_col = get_fog_color(water_col, ray.dist, sundot);
        water_col = mix(
            water_col,
            get_sky_color(direction, ray.dist, sundot),
            fog_mix(ray.dist / max_dist)
        );
        color = water_col;
    } 
    // hit - get terrain
    else {
        color = get_terrain_color(ray, direction, sundot);
        // TODO: draw particles
        for (uint i = 0; i < PARTICLE_COUNT; i++) {
            // ivec2 part_pos = ivec2(particles[i].position / float(set.hmap_dims.xy) * (gl_NumWorkGroups.xy * gl_WorkGroupSize.xy));
            vec2 part_pos = particles[i].position;
            if (abs(length(ray.pos.xz - part_pos)) <= particles[i].volume) {
                color = water_color;
            }
        }
    }

    if (display_sediment) {
        float r = min(1.0, img_bilinear_r(sedimentmap, ray.pos.xz) / sediment_max_cap);
        float g = min(1.0, img_bilinear_g(sedimentmap, ray.pos.xz) / sediment_max_cap);
        color = mix(color, vec3(1,0,0), r);
        color = mix(color, vec3(0,1,0), g);
    }
    return color;
}

Ray raymarch(
    vec3 origin, vec3 direction, 
    const float max_dist, const int max_steps, 
    const int terr_type
) {
    float dist = 0.001;
    float d_dist = 0.1;

    if (terr_type == TERRAIN) {
        dist = 0.0;
        d_dist = 0.1e-12;
    }

    // shadow penumbra
    float min_sdf = 1.0;
    float t_height = 0.0;
    vec4 terr = vec4(0.0);

    for (int i = 0; i < max_steps; ++i) {
        vec3 sample_pos = origin + direction * dist;

        terr = img_bilinear(heightmap, sample_pos.xz);

        if (terr_type == TERRAIN) {
            t_height = terr.r + terr.g;
        } 
        // ELSE TOTAL
        else {
            t_height = terr.w;
        }

        if (
            (sample_pos.y > set.max_height && direction.y >= 0) ||
            (sample_pos.y <= 0 && direction.y < 0)
        ) {
            break;
        }
        // hitting the ground ??
        float d_height = sample_pos.y - t_height;
        if (abs(d_height) <= 0.05) {
            return Ray(
                vec4(sample_pos - (0.05 * direction), 0.0),
                dist - (0.05),
                img_bilinear(heightmap, (sample_pos - (0.05 * direction)).xz)
            );
        }
        else if (d_height < 0) {
            return Ray(vec4(origin + direction * dist, min_sdf), dist, terr);
        }
        // penumbra
        min_sdf = min(min_sdf, 12 * d_dist / dist);
        if (dist > max_dist) {
            return Ray(
                vec4(sample_pos, min_sdf),
                max_dist,
                terr
            );
        }
        // TODO: dist can't be higher than max slope approximation 
        d_dist = prec * d_height;
        dist += d_dist;
    }
    return Ray(
        vec4(origin + direction * max_dist, min_sdf),
        max_dist,
        terr
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

    if (DEBUG_PREVIEW) {
        imageStore(
            out_tex, pixel, vec4(
                (img_bilinear_r(heightmap, vec2(pixel)) + 
                img_bilinear_g(heightmap, vec2(pixel))) / set.max_height,
                img_bilinear_g(sedimentmap, vec2(pixel)) / sediment_max_cap,
                img_bilinear_b(heightmap, vec2(pixel)),
                0
            )
        );
        return;
    }
    vec2 clip = 2.0 * uv - 1.0;

    vec4 ray_start  = to_world(vec4(0.0, 0.0, -1.0, 1.0));
    vec4 ray_end    = to_world(vec4(clip, 1.0, 1.0));
    vec3 ray_dir = 
        normalize(ray_end.xyz - ray_start.xyz);

    vec3 color = get_pixel_color(pos, ray_dir);
    // gamma correction
    color = pow(color, vec3(1.0 / 2.2));

#ifdef LOW_RES_DIV3
    imageStore(out_tex, pixel * 3, vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(1, 0), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(2, 0), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(0, 1), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(0, 2), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(1, 1), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(2, 1), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(1, 2), vec4(color, set.hmap_dims.x));
    imageStore(out_tex, pixel * 3 + ivec2(2, 2), vec4(color, set.hmap_dims.x));
#else
    imageStore(out_tex, pixel, vec4(color, 0));
#endif
}
