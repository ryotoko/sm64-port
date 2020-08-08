#version 110

varying vec2      vTexCoord;
varying vec4      vFog;
varying vec4      vInput1;
varying vec4      vInput2;
varying vec4      vInput3;
varying vec4      vInput4;

uniform sampler2D uTex0;
uniform sampler2D uTex1;

uniform int       frame_count;
uniform int       window_height;

uniform uint      tex_flags;
uniform bool      fog_used;
uniform bool      alpha_used;
uniform bool      noise_used;
uniform bool      texture_edge;
uniform bool      color_alpha_same;

uniform uvec4     c[2];
uniform bool      do_single[2];
uniform bool      do_multiply[2];
uniform bool      do_mix[2];

vec4 texVal0 = vec4(0.0, 0.0, 0.0, 0.0);
vec4 texVal1 = vec4(0.0, 0.0, 0.0, 0.0);

float random(in vec3 value) {
    float random = dot(sin(value), vec3(12.9898, 78.233, 37.719));
    return fract(sin(random) * 143758.5453);
}

vec3 get_color(uint item) {
    switch (item) {
        case 0: // SHADER_0
            return vec3(0.0, 0.0, 0.0);
        case 1: // SHADER_INPUT_1
            return vInput1.rgb;
        case 2: // SHADER_INPUT_2
            return vInput2.rgb;
        case 3: // SHADER_INPUT_3
            return vInput3.rgb;
        case 4: // SHADER_INPUT_4
            return vInput4.rgb;
        case 5: // SHADER_TEXEL0
            return texVal0.rgb;
        case 6: // SHADER_TEXEL0A
            return vec3(texVal0.a, texVal0.a, texVal0.a);
        case 7: // SHADER_TEXEL1
            return texVal1.rgb;
    }

    return vec3(0.0, 0.0, 0.0);
}

float get_alpha(uint item) {
    switch (item) {
        case 0: //SHADER_0
            return 0.0;
        case 1: // SHADER_INPUT_1
            return vInput1.a;
        case 2: // SHADER_INPUT_2
            return vInput2.a;
        case 3: // SHADER_INPUT_3
            return vInput3.a;
        case 4: // SHADER_INPUT_4
            return vInput4.a;
        case 5: // SHADER_TEXEL0
            return texVal0.a;
        case 6: // SHADER_TEXEL0A
            return texVal0.a;
        case 7: // SHADER_TEXEL1
            return texVal1.a;
    }

    return 0.0;

}

vec4 get_color_alpha(uint item) {
    switch (item) {
        case 0: // SHADER_0
            return vec4(0.0, 0.0, 0.0, 0.0);
        case 1: // SHADER_INPUT_1
            return vInput1;
        case 2: // SHADER_INPUT_2
            return vInput2;
        case 3: // SHADER_INPUT_3
            return vInput3;
        case 4: // SHADER_INPUT_4
            return vInput4;
        case 5: // SHADER_TEXEL0
            return texVal0;
        case 6: // SHADER_TEXEL0A
            return vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a);
        case 7: // SHADER_TEXEL1
            return texVal1;
    }

    return vec4(0.0, 0.0, 0.0, 0.0);
}

vec4 get_texel() {
    if (alpha_used) {
        if (!color_alpha_same) {
            vec3 color;
            float alpha;

            if (do_single[0]) {
                color = get_color(c[0][3]);
            } else if (do_multiply[0]) {
                color = get_color(c[0][0]) * get_color(c[0][2]);
            } else if (do_mix[0]) {
                color = mix(get_color(c[0][1]), get_color(c[0][0]), get_color(c[0][2]));
            } else {
                color = (get_color(c[0][0]) - get_color(c[0][1])) * get_color(c[0][2]) + get_color(c[0][3]);
            }

            if (do_single[1]) {
                alpha = get_alpha(c[1][3]);
            } else if (do_multiply[1]) {
                alpha = get_alpha(c[1][0]) * get_alpha(c[1][2]);
            } else if (do_mix[1]) {
                alpha = mix(get_alpha(c[1][1]), get_alpha(c[1][0]), get_alpha(c[1][2]));
            } else {
                alpha = (get_alpha(c[1][0]) - get_alpha(c[1][1])) * get_alpha(c[1][2]) + get_alpha(c[1][3]);
            }

            return vec4(color, alpha);
        } else {
            if (do_single[0]) {
                return get_color_alpha(c[0][3]);
            } else if (do_multiply[0]) {
                return get_color_alpha(c[0][0]) * get_color_alpha(c[0][2]);
            } else if (do_mix[0]) {
                return mix(get_color_alpha(c[0][1]), get_color_alpha(c[0][0]), get_color_alpha(c[0][2]));
            } else {
                return (get_color_alpha(c[0][0]) - get_color_alpha(c[0][1])) * get_color_alpha(c[0][2]) + get_color_alpha(c[0][3]);
            }
        }
    } else {
        if (do_single[0]) {
            return vec4(get_color(c[0][3]), 1.0);
        } else if (do_multiply[0]) {
            return vec4(get_color(c[0][0]) * get_color(c[0][2]), 1.0);
        } else if (do_mix[0]) {
            return vec4(mix(get_color(c[0][1]), get_color(c[0][0]), get_color(c[0][2])), 1.0);
        } else {
            return vec4((get_color(c[0][0]) - get_color(c[0][1])) * get_color(c[0][2]) + get_color(c[0][3]), 1.0);
        }
    }
}

void main() {
    switch (tex_flags) {
        case 1:
            texVal0 = texture2D(uTex0, vTexCoord);
            break;
        case 2:
            texVal1 = texture2D(uTex1, vTexCoord);
            break;
        case 3:
            texVal0 = texture2D(uTex0, vTexCoord);
            texVal1 = texture2D(uTex1, vTexCoord);
            break;
    }

    vec4 texel = get_texel();
    if (alpha_used && texture_edge && texel.a > 0.3) {
        texel.a = 1.0;
    }

    if (fog_used) {
        texel.rgb = mix(texel.rgb, vFog.rgb, vFog.a);
    }

    if (alpha_used && noise_used) {
        texel.a *= floor(random(vec3(floor(gl_FragCoord.xy * (240.0 / float(window_height))), float(frame_count))) + 0.5);
    }

    gl_FragColor = texel;
}
