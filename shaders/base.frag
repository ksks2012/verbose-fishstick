#version 330 core

// Pass-through varyings from vertex stage
in vec3 vNormal;
in vec2 vUV;

uniform vec4      uBaseColor;    // linear RGBA (ADR-0004)
uniform float     uOpacity;      // Material::opacity
// 1 = diffuse texture is bound to unit 0; 0 = untextured (use baseColor only)
uniform int       uHasTexture;
// GL_SRGB8_ALPHA8 diffuse — hardware sRGB→linear decode on sample (ADR-0004)
uniform sampler2D uDiffuse;

out vec4 FragColor;

void main() {
    // effectiveAlpha = baseColor.a * opacity  (DEFINITION.md; unchanged by 0009)
    float effectiveAlpha = uBaseColor.a * uOpacity;

    // Sample diffuse when bound; hardware decodes sRGB→linear via GL_SRGB8_ALPHA8.
    // When no texture is bound uHasTexture==0 → white (1,1,1,1) preserves baseColor.
    vec4 texSample = (uHasTexture != 0) ? texture(uDiffuse, vUV) : vec4(1.0);

    // finalAlpha = effectiveAlpha * sampledAlpha  (ticket 0009)
    float finalAlpha = effectiveAlpha * texSample.a;

    // Output linear RGB; sRGB framebuffer converts linear→sRGB on output (ADR-0004).
    FragColor = vec4(uBaseColor.rgb * texSample.rgb, finalAlpha);
}
