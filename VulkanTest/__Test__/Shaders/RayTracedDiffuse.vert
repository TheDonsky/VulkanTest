#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Transform {
	mat4 inverseView;
	mat4 inverseProjection;
} inverseTransform;

layout(location = 0) in vec3 screenPosition;

// Origin will be the same for every vertex, but using this vertex&fragment shader hack is not the best thing ever anyway, 
// considering a compute shader woul do just fine instead, but it's ok for test I guess...
layout(location = 0) out vec3 rayOrigin;
layout(location = 1) out vec3 rayDirection;

void main() {
	gl_Position = vec4(screenPosition, 1.0f);
	vec4 origin = inverseTransform.inverseView * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	rayOrigin = vec3(origin.x, origin.y, origin.z) / origin.w;
	vec4 direction = inverseTransform.inverseView * inverseTransform.inverseProjection * vec4(screenPosition, 1.0f);
	rayDirection = (vec3(direction.x, direction.y, direction.z) / direction.w - rayOrigin);
}
