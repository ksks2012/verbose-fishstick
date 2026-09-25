#version 330 core

// Pass-through varyings from vertex stage
in vec3 vNormal;
in vec2 vUV;

uniform vec4      uBaseColor;    // linear RGBA (ADR-0004)
uniform float     uOpacity;      // Material::opacity
// true = diffuse texture is bound to unit 0; false = untextured (use baseColor only)
uniform bool      uHasTexture;
// GL_SRGB8_ALPHA8 diffuse — hardware sRGB→linear decode on sample (ADR-0004)
uniform sampler2D uDiffuse;

out vec4 FragColor;

void main() {
    // effectiveAlpha = baseColor.a * opacity  (DEFINITION.md; unchanged by 0009)
    float effectiveAlpha = uBaseColor.a * uOpacity;

    // MVP uHasTexture branch: no texture → sampledAlpha=1.0, texColor=vec4(1.0)
    // When uHasTexture is false, uDiffuse is not sampled (its bind is irrelevant).
    vec4  texColor     = vec4(1.0);
    float sampledAlpha = 1.0;
    if (uHasTexture) {
        texColor     = texture(uDiffuse, vUV);
        sampledAlpha = texColor.a;
    }

    // finalAlpha = effectiveAlpha * sampledAlpha  (ticket 0009)
    float finalAlpha = effectiveAlpha * sampledAlpha;

    // finalColor.rgb = baseColor.rgb * texColor.rgb  (linear × linear; ADR-0004)
    // Output linear RGB; sRGB framebuffer converts linear→sRGB on output.
    FragColor = vec4(uBaseColor.rgb * texColor.rgb, finalAlpha);
}
