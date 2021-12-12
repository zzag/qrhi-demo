#version 440

layout(location = 0) in vec2 coord;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec2 outTexCoord;

void main()
{
    gl_Position = vec4(coord, 0.0, 1.0);
    outTexCoord = texCoord;
}
