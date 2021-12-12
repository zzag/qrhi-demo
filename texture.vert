#version 440

layout(location = 0) in vec2 coord;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec2 out_texCoord;

layout(std140, binding = 0) uniform buf {
    mat4 modelViewProjectionMatrix;
};

void main()
{
    gl_Position = modelViewProjectionMatrix * vec4(coord, 0.0, 1.0);
    out_texCoord = texCoord;
}
