#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform float uRadius;
uniform float uSoftness;

void main(){
	vec4 color = texture(uScene, vUV);
	vec2 offset = vUV - 0.5;
	float dist = length(offset);
	float vignette = smoothstep(uRadius, uRadius - uSoftness, dist);
	FragColor = vec4(color.rgb * vignette, color.a);
}