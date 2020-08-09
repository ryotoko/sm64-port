
#version 130

attribute vec4 aVtxPos;

attribute vec2 aTexCoord;
varying   vec2 vTexCoord;

attribute vec4 aFog;
varying   vec4 vFog;

attribute vec4 aInput1;
varying   vec4 vInput1;

attribute vec4 aInput2;
varying   vec4 vInput2;

attribute vec4 aInput3;
varying   vec4 vInput3;

attribute vec4 aInput4;
varying   vec4 vInput4;

uniform   int  tex_flags;
uniform   bool fog_used;
uniform   int  num_inputs;

void main() {
    if (tex_flags != 0) {
        vTexCoord = aTexCoord;
    }

    if (fog_used) {
        vFog = aFog;
    }

    switch (num_inputs) {
        case 4:
            vInput4 = aInput4;
        case 3:
            vInput3 = aInput3;
        case 2:
            vInput2 = aInput2;
        case 1:
            vInput1 = aInput1;
    }

    gl_Position = aVtxPos;
}
