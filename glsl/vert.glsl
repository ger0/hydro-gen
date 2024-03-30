#version 460 core
layout (location = 0)in vec3 vertex;
layout (location = 1)in vec3 normal_old;
vec3 normal = vec3(0.0, 1.0, 0.0);
// layout (location = 2)in vec4 color;
vec4 color = vec4(1.0, 1.0, 1.0, 1.0);
//layout (location = 2)in unsigned blockType;

uniform mat4 P, V, M;

out vec3 iNormal;
out vec3 vertPos;
out vec4 iColor;
out float visibility;

uniform float density;
uniform float gradient;

void main() {
	gl_Position = P * V * M * vec4(vertex, 1.0);
	vec4 vertPos4 = M * vec4(vertex, 1.0);
	vec4 viewVertPos4 = V * M * vec4(vertex, 1.0);
	vertPos = vec3(vertPos4);
	iNormal = vec3(M * vec4(normal, 0.0));

	iColor = color;
	float distance = length(viewVertPos4.xyz);
	visibility = exp(-pow((distance * density), gradient));
	visibility = clamp(visibility, 0.0, 1.0);
}
