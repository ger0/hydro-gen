#version 460
// source: https://github.com/FarazzShaikh/glNoise
// #name: BlendModes

#define gln_COPY 1
#define gln_ADD 2
#define gln_SUBSTRACT 3
#define gln_MULTIPLY 4
#define gln_ADDSUB 5
#define gln_LIGHTEN 6
#define gln_DARKEN 7
#define gln_SWITCH 8
#define gln_DIVIDE 9
#define gln_OVERLAY 10
#define gln_SCREEN 11
#define gln_SOFTLIGHT 12

float gln_softLight(float f, float b) {
    return (f < 0.5)
    ? b - (1.0 - 2.0 * f) * b * (1.0 - b)
    : (b < 0.25)
    ? b + (2.0 * f - 1.0) * b * ((16.0 * b - 12.0) * b + 3.0)
    : b + (2.0 * f - 1.0) * (sqrt(b) - b);
}

vec4 gln_softLight(vec4 f, vec4 b) {
    vec4 result;
    result.x = gln_softLight(f.x, b.x);
    result.y = gln_softLight(f.y, b.y);
    result.z = gln_softLight(f.z, b.z);
    result.a = gln_softLight(f.a, b.a);
    return result;
}

vec4 gln_screen(vec4 f, vec4 b) {
    vec4 result;

    result = 1.0 - (1.0 - f) * (1.0 - b);

    return result;
}

float gln_overlay(float f, float b) {
    return (b < 0.5) ? 2.0 * f * b : 1.0 - 2.0 * (1.0 - f) * (1.0 - b);
}

vec4 gln_overlay(vec4 f, vec4 b) {
    vec4 result;
    result.x = gln_overlay(f.x, b.x);
    result.y = gln_overlay(f.y, b.y);
    result.z = gln_overlay(f.z, b.z);
    result.a = gln_overlay(f.a, b.a);
    return result;
}

vec4 gln_divide(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result = b / f;

    return result;
}

vec4 gln_switch(vec4 f, vec4 b, float o) {
    vec4 result = vec4(0.0);

    result = max((f * o), (b * (1.0 - o)));

    return result;
}

vec4 gln_darken(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result = min(f, b);

    return result;
}

vec4 gln_lighten(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result = max(f, b);

    return result;
}

float gln_addSub(float f, float b) { return f > 0.5 ? f + b : b - f; }

vec4 gln_addSub(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result.r = gln_addSub(f.r, b.r);
    result.g = gln_addSub(f.g, b.g);
    result.b = gln_addSub(f.b, b.b);
    result.a = gln_addSub(f.a, b.a);

    return result;
}

vec4 gln_multiply(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result = f * b;
    result.a = f.a + b.a * (1.0 - f.a);

    return result;
}

vec4 gln_subtract(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result = b - f;
    result.a = f.a + b.a * (1.0 - f.a);

    return result;
}

vec4 gln_add(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result = f + b;
    result.a = f.a + b.a * (1.0 - f.a);

    return result;
}

vec4 gln_copy(vec4 f, vec4 b) {
    vec4 result = vec4(0.0);

    result.a = f.a + b.a * (1.0 - f.a);
    result.rgb = ((f.rgb * f.a) + (b.rgb * b.a) * (1.0 - f.a));

    return result;
}

/**
 * Enum for gl-Noise Blend Modes.
 * @name gln_BLENDMODES
 * @enum {number}
 * @property {number} gln_COPY The <b>Copy</b> blending mode will just place the
 * foreground on top of the background.
 * @property {number} gln_ADD The <b>Add</b> blending mode will add the
 * foreground input value to each corresponding pixel in the background.
 * @property {number} gln_SUBSTRACT The <b>Substract</b> blending mode will
 * substract the foreground input value from each corresponding pixel in the
 * background.
 * @property {number} gln_MULTIPLY The <b>Multiply</b> blending mode will
 * multiply the background input value by each corresponding pixel in the
 * foreground.
 * @property {number} gln_ADDSUB The <b>Add Sub</b> blending mode works as
 * following: <ul> <li> Foreground pixels with a value higher than 0.5 are added
 * to their respective background pixels. </li> <li> Foreground pixels with a
 * value lower than 0.5 are substracted from their respective background pixels.
 * </li>
 * </ul>
 * @property {number} gln_LIGHTEN The <b>Lighten (Max)</b> Blending mode will
 * pick the higher value between the background and the foreground.
 * @property {number} gln_DARKEN The <b>Darken (Min)</b> Blending mode will pick
 * the lower value between the background and the foreground.
 * @property {number} gln_DIVIDE The <b>Divide</b> blending mode will divide the
 * background input pixels value by each corresponding pixel in the foreground.
 * @property {number} gln_OVERLAY The <b>Overlay</b> blending mode combines
 * Multiply and Screen blend modes: <ul> <li> If the value of the lower layer
 * pixel is below 0.5, then a Multiply type blending is applied. </li> <li> If
 * the value of the lower layer pixel is above 0.5, then a Screen type blending
 * is applied. </li>
 * </ul>
 * @property {number} gln_SCREEN With <b>Screen</b> blend mode the values of the
 * pixels in the two inputs are inverted, multiplied, and then inverted
 * again.</br>The result is the opposite effect to multiply and is always equal
 * or higher (brighter) compared to the original.
 * @property {number} gln_SOFTLIGHT The <b>Soft Light</b> blend mode creates a
 * subtle lighter or darker result depending on the brightness of the foreground
 * color.
 * </br>Blend colors that are more than 50% brightness will lighten the
 * background pixels and colors that are less than 50% brightness will darken
 * the background pixels.
 */

/**
 * Blends a Color with another.
 *
 * @name gln_blend
 * @function
 * @param {vec4} f  Foreground
 * @param {vec4} b  Background
 * @param {gln_BLENDMODES} type  Blend mode
 * @return {vec4} Mapped Value
 *
 * @example
 * vec4 logo = texture2D(uLogo, uv);
 * vec4 rect = texture2D(uRect, uv);
 *
 * vec4 final = gln_blend(logo, rect, gln_COPY);
 */
vec4 gln_blend(vec4 f, vec4 b, int type) {

    vec4 n;

    if (type == gln_COPY) {
        n = gln_copy(f, b);
    } else if (type == gln_ADD) {
        n = gln_add(f, b);
    } else if (type == gln_SUBSTRACT) {
        n = gln_subtract(f, b);
    } else if (type == gln_MULTIPLY) {
        n = gln_multiply(f, b);
    } else if (type == gln_ADDSUB) {
        n = gln_addSub(f, b);
    } else if (type == gln_LIGHTEN) {
        n = gln_lighten(f, b);
    } else if (type == gln_DARKEN) {
        n = gln_darken(f, b);
    } else if (type == gln_DIVIDE) {
        n = gln_divide(f, b);
    } else if (type == gln_OVERLAY) {
        n = gln_overlay(f, b);
    } else if (type == gln_SCREEN) {
        n = gln_screen(f, b);
    } else if (type == gln_SOFTLIGHT) {
        n = gln_softLight(f, b);
    }

    return n;
}
// #name: Common

#define MAX_FBM_ITERATIONS 30
#define gln_PI 3.1415926538
vec4 _permute(vec4 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }
vec4 _taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

/**
 * @typedef {struct} gln_tFBMOpts   Options for fBm generators.
 * @property {float} seed           Seed for PRNG generation.
 * @property {float} persistance    Factor by which successive layers of noise
 * will decrease in amplitude.
 * @property {float} lacunarity     Factor by which successive layers of noise
 * will increase in frequency.
 * @property {float} scale          "Zoom level" of generated noise.
 * @property {float} redistribution Flatness in the generated noise.
 * @property {int} octaves          Number of layers of noise to stack.
 * @property {boolean} terbulance   Enable terbulance
 * @property {boolean} ridge        Convert the fBm to Ridge Noise. Only works
 * when "terbulance" is set to true.
 */
struct gln_tFBMOpts {
    float seed;
    float persistance;
    float lacunarity;
    float scale;
    float redistribution;
    int octaves;
    bool terbulance;
    bool ridge;
};

/**
 * Converts a number from one range to another.
 *
 * @name gln_map
 * @function
 * @param {} value      Value to map
 * @param {float} min1  Minimum for current range
 * @param {float} max1  Maximum for current range
 * @param {float} min2  Minimum for wanted range
 * @param {float} max2  Maximum for wanted range
 * @return {float} Mapped Value
 *
 * @example
 * float n = gln_map(-0.2, -1.0, 1.0, 0.0, 1.0);
 * // n = 0.4
 */
float gln_map(float value, float min1, float max1, float min2, float max2) {
    return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

/**
 * Normalized a value from the range [-1, 1] to the range [0,1].
 *
 * @name gln_normalize
 * @function
 * @param {float} v Value to normalize
 * @return {float} Normalized Value
 *
 * @example
 * float n = gln_normalize(-0.2);
 * // n = 0.4
 */
float gln_normalize(float v) { return gln_map(v, -1.0, 1.0, 0.0, 1.0); }

/**
 * Generates a random 2D Vector.
 *
 * @name gln_rand2
 * @function
 * @param {vec2} p Vector to hash to generate the random numbers from.
 * @return {vec2} Random vector.
 *
 * @example
 * vec2 n = gln_rand2(vec2(1.0, -4.2));
 */
vec2 gln_rand2(vec2 p) {
    return fract(
        sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) *
        43758.5453);
}

/**
 * Generates a random 3D Vector.
 *
 * @name gln_rand3
 * @function
 * @param {vec3} p Vector to hash to generate the random numbers from.
 * @return {vec3} Random vector.
 *
 * @example
 * vec3 n = gln_rand3(vec3(1.0, -4.2, 0.2));
 */
vec3 gln_rand3(vec3 p) { return mod(((p * 34.0) + 1.0) * p, 289.0); }

/**
 * Generates a random 4D Vector.
 *
 * @name gln_rand4
 * @function
 * @param {vec4} p Vector to hash to generate the random numbers from.
 * @return {vec4} Random vector.
 *
 * @example
 * vec4 n = gln_rand4(vec4(1.0, -4.2, 0.2, 2.2));
 */
vec4 gln_rand4(vec4 p) { return mod(((p * 34.0) + 1.0) * p, 289.0); }

/**
 * Generates a random number.
 *
 * @name gln_rand
 * @function
 * @param {float} n Value to hash to generate the number from.
 * @return {float} Random number.
 *
 * @example
 * float n = gln_rand(2.5);
 */
float gln_rand(float n) { return fract(sin(n) * 1e4); }

/**
 * Generates a random number.
 *
 * @name gln_rand
 * @function
 * @param {vec2} p Value to hash to generate the number from.
 * @return {float} Random number.
 *
 * @example
 * float n = gln_rand(vec2(2.5, -1.8));
 */
float gln_rand(vec2 p) {
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) *
                 (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

// #name: Simplex

/**
 * Generates 2D Simplex Noise.
 *
 * @name gln_simplex
 * @function
 * @param {vec2} v  Point to sample Simplex Noise at.
 * @return {float}  Value of Simplex Noise at point "p".
 *
 * @example
 * float n = gln_simplex(position.xy);
 */
float gln_simplex(vec2 v) {
    const vec4 C = vec4(0.211324865405187, 0.366025403784439, -0.577350269189626,
                        0.024390243902439);
    vec2 i = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);
    vec2 i1;
    i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    vec4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i = mod(i, 289.0);
    vec3 p = gln_rand3(gln_rand3(i.y + vec3(0.0, i1.y, 1.0)) + i.x +
                       vec3(0.0, i1.x, 1.0));
    vec3 m = max(
        0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);
    vec3 g;
    g.x = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g);
}

/**
 * Generates 3D Simplex Noise.
 *
 * @name gln_simplex
 * @function
 * @param {vec3} v  Point to sample Simplex Noise at.
 * @return {float}  Value of Simplex Noise at point "p".
 *
 * @example
 * float n = gln_simplex(position.xyz);
 */
float gln_simplex(vec3 v) {
    const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner
    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);

    // Other corners
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    //  x0 = x0 - 0. + 0.0 * C
    vec3 x1 = x0 - i1 + 1.0 * C.xxx;
    vec3 x2 = x0 - i2 + 2.0 * C.xxx;
    vec3 x3 = x0 - 1. + 3.0 * C.xxx;

    // Permutations
    i = mod(i, 289.0);
    vec4 p = _permute(_permute(_permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y +
                               vec4(0.0, i1.y, i2.y, 1.0)) +
                      i.x + vec4(0.0, i1.x, i2.x, 1.0));

    // Gradients
    // ( N*N points uniformly over a square, mapped onto an octahedron.)
    float n_ = 1.0 / 7.0; // N=7
    vec3 ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z); //  mod(p,N*N)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_); // mod(j,N)

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    // Normalise gradients
    vec4 norm =
        _taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value
    vec4 m =
        max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;
    return 42.0 *
    dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

/**
 * Generates 2D Fractional Brownian motion (fBm) from Simplex Noise.
 *
 * @name gln_sfbm
 * @function
 * @param {vec2} v               Point to sample fBm at.
 * @param {gln_tFBMOpts} opts    Options for generating Simplex Noise.
 * @return {float}               Value of fBm at point "p".
 *
 * @example
 * gln_tFBMOpts opts =
 *      gln_tFBMOpts(uSeed, 0.3, 2.0, 0.5, 1.0, 5, false, false);
 *
 * float n = gln_sfbm(position.xy, opts);
 */
float gln_sfbm(vec2 v, gln_tFBMOpts opts) {
    v += (opts.seed * 100.0);
    float persistance = opts.persistance;
    float lacunarity = opts.lacunarity;
    float redistribution = opts.redistribution;
    int octaves = opts.octaves;
    bool terbulance = opts.terbulance;
    bool ridge = opts.terbulance && opts.ridge;

    float result = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maximum = amplitude;

    for (int i = 0; i < MAX_FBM_ITERATIONS; i++) {
        if (i >= octaves)
            break;

        vec2 p = v * frequency * opts.scale;

        float noiseVal = gln_simplex(p);

        if (terbulance)
            noiseVal = abs(noiseVal);

        if (ridge)
            noiseVal = -1.0 * noiseVal;

        result += noiseVal * amplitude;

        frequency *= lacunarity;
        amplitude *= persistance;
        maximum += amplitude;
    }

    float redistributed = pow(result, redistribution);
    return redistributed / maximum;
}

/**
 * Generates 3D Fractional Brownian motion (fBm) from Simplex Noise.
 *
 * @name gln_sfbm
 * @function
 * @param {vec3} v               Point to sample fBm at.
 * @param {gln_tFBMOpts} opts    Options for generating Simplex Noise.
 * @return {float}               Value of fBm at point "p".
 *
 * @example
 * gln_tFBMOpts opts =
 *      gln_tFBMOpts(uSeed, 0.3, 2.0, 0.5, 1.0, 5, false, false);
 *
 * float n = gln_sfbm(position.xy, opts);
 */
float gln_sfbm(vec3 v, gln_tFBMOpts opts) {
    v += (opts.seed * 100.0);
    float persistance = opts.persistance;
    float lacunarity = opts.lacunarity;
    float redistribution = opts.redistribution;
    int octaves = opts.octaves;
    bool terbulance = opts.terbulance;
    bool ridge = opts.terbulance && opts.ridge;

    float result = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maximum = amplitude;

    for (int i = 0; i < MAX_FBM_ITERATIONS; i++) {
        if (i >= octaves)
            break;

        vec3 p = v * frequency * opts.scale;

        float noiseVal = gln_simplex(p);

        if (terbulance)
            noiseVal = abs(noiseVal);

        if (ridge)
            noiseVal = -1.0 * noiseVal;

        result += noiseVal * amplitude;

        frequency *= lacunarity;
        amplitude *= persistance;
        maximum += amplitude;
    }

    float redistributed = pow(result, redistribution);
    return redistributed / maximum;
}
