#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform vec2      uResolution;  // set by PostProcessStack
uniform float     uPixelSize;   // default: 4.0

void main() {
    // Number of pixel blocks across each axis
    vec2 blockCount = uResolution / max(uPixelSize, 1.0);

    // Snap UV to the center of the nearest block
    vec2 snapped = (floor(vUV * blockCount) + 0.5) / blockCount;

    FragColor = texture(uScene, snapped);
}