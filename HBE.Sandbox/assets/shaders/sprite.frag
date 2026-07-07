#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec4 uColor;

uniform int uIsSDF;
uniform float uSDFSoftness;

void main() {
    vec4 tex = texture(uTex, vUV);

    if (uIsSDF == 0) {
        FragColor = tex * uColor * vColor;
        return;
    }

    float dist = tex.a;
    float w = fwidth(dist) * max(uSDFSoftness, 0.001);
    float alpha = smoothstep(0.5 - w, 0.5 + w, dist);
    vec4 tinted = uColor * vColor;
    FragColor = vec4(tinted.rgb, tinted.a * alpha);
}