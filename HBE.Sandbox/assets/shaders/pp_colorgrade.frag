#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;

void main(){
	vec4 color = texture(uScene, vUV);
	vec3 c = color.rgb;
	c = (c - 0.5) * uContrast + 0.5;
	c += uBrightness;
	float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
	c = mix(vec3(lum), c, uSaturation);
	FragColor = vec4(clamp(c, 0.0, 1.0), color.a);
}