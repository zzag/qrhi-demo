#version 440

layout(location = 0) in vec2 coord;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec2 outTexCoord;

layout(std140, binding = 1) uniform buf {
    mat4 mvp;
};

void main()
{
    gl_Position = mvp * vec4(coord, 0.0, 1.0);
    outTexCoord = texCoord;
}
