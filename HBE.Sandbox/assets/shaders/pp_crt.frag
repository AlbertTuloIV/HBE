#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform vec2      uResolution;       // set by PostProcessStack
uniform float     uScanlineStrength; // default: 0.3
uniform float     uCurvature;        // default: 4.0

// Barrel warp: bows the UV inward like a CRT tube
vec2 curve(vec2 uv, float k) {
    uv = uv * 2.0 - 1.0;
    uv *= 1.0 + vec2(uv.y * uv.y, uv.x * uv.x) * (1.0 / k);
    return (uv * 0.5) + 0.5;
}

void main() {
    vec2 curved = curve(vUV, uCurvature);

    // Hard black border outside the warped area
    if (curved.x < 0.0 || curved.x > 1.0 ||
        curved.y < 0.0 || curved.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 color = texture(uScene, curved);

    // Horizontal scanlines: a sine wave in Y, one cycle per logical pixel row
    float line    = sin(curved.y * uResolution.y * 3.14159265);
    float scanline = mix(1.0, line * 0.5 + 0.5, uScanlineStrength);
    color.rgb *= scanline;

    FragColor = color;
}