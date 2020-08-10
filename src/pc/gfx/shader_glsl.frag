
#version 130

varying vec2      vTexCoord;

uniform sampler2D uTex0;
uniform sampler2D uTex1;

void main() {
    vec4 texVal0 = texture(uTex0, vTexCoord);
    vec4 texVal1 = texture(uTex1, vTexCoord);

    gl_FragColor = vec4(texVal0.rgb + texVal1.rgb, 1.0);
}
