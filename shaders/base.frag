#version 330 core

// Pass-through varyings from vertex stage (used by M2+ shaders)
in vec3 vNormal;
in vec2 vUV;

uniform vec4  uBaseColor;  // linear RGBA (ADR-0004); sRGB conversion by framebuffer
uniform float uOpacity;    // Material::opacity

out vec4 FragColor;

void main() {
    // effectiveAlpha = baseColor.a * opacity  (DEFINITION.md)
    float effectiveAlpha = uBaseColor.a * uOpacity;
    // Output linear color — sRGB framebuffer converts linear→sRGB on output (ADR-0004).
    // M1 base shader does NOT sample Material::texture (deferred to ticket 0009).
    FragColor = vec4(uBaseColor.rgb, effectiveAlpha);
}
