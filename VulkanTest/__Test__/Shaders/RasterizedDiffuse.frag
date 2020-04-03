#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform Light {
	vec3 position;
	vec3 color;
	vec3 ambientStrength;
} light;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 worldPos;
layout(location = 2) in vec3 pixelColor;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 deltaPos = (light.position - worldPos);
	float sqrDistance = dot(deltaPos, deltaPos);
	vec3 color = (light.color / sqrDistance);
	vec3 dirToLight = (deltaPos / sqrt(sqrDistance));
	float tangent = dot(dirToLight, fragNormal);
	vec3 conserved;
	if (tangent <= 0) conserved = light.ambientStrength;
	else conserved = (light.ambientStrength + tangent); 
	outColor = vec4(pixelColor * color * conserved, 1.0f);
}
