#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D tex;

void main()
{
    fragColor = pow(texture(tex, texCoord), vec4(0.75)) * vec4(0.5, 0.8, 0.9, 1.0);
}
