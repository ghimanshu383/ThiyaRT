#version 450

layout (location = 0) in vec2 vertUv;
layout (location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D computeImg;
void main() {
    color = texture(computeImg, vertUv);
   // color = vec4(1, 1, 0, 0);
}