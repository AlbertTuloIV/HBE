#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform vec2      uResolution;  // logical size (set automatically by PostProcessStack)
uniform float     uThreshold;   // default: 0.7
uniform float     uIntensity;   // default: 1.2

void main() {
    vec4 base = texture(uScene, vUV);
    vec2 texel = 1.0 / uResolution;

    vec3  bloom  = vec3(0.0);
    float weight = 0.0;
    const int radius = 3;

    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            vec2  off = vec2(float(x), float(y)) * texel;
            vec3  s   = texture(uScene, vUV + off).rgb;
            float lum = dot(s, vec3(0.2126, 0.7152, 0.0722));
            if (lum > uThreshold) {
                bloom  += s;
                weight += 1.0;
            }
        }
    }
    if (weight > 0.0) bloom /= weight;

    FragColor = vec4(base.rgb + bloom * uIntensity, base.a);
}