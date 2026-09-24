#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uMVP;

// Pass-through varyings — consumed by M2+ shaders (lighting, texture)
out vec3 vNormal;
out vec2 vUV;

void main() {
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vNormal     = aNormal;
    vUV         = aUV;
}
