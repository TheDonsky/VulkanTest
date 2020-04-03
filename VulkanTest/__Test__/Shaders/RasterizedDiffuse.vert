#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Transform {
	mat4 view;
	mat4 projection;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 worldPos;
layout(location = 2) out vec3 vertColor;

void main() {
	gl_Position = transform.projection * transform.view * vec4(inPosition, 1.0);
	fragNormal = inNormal;
	worldPos = inPosition;
	vertColor = inColor;
}
