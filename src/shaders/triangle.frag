#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
  float gamma = 2.2f; 
  outColor = vec4(pow(fragColor, 1.0f/gamma.xxx), 1.0);
}