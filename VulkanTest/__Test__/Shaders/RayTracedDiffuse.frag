#version 450
#extension GL_ARB_separate_shader_objects : enable

struct PNCVertex {
	vec3 position;
	vec3 normal;
	vec3 color;
};

struct Triangle {
	PNCVertex a, b, c;
};

struct PosTriangle {
	vec3 a, b, c;
};

struct Ray {
	vec3 origin, direction;
};

layout (std430, binding = 1) buffer readonly VertexBuffer {
	PNCVertex vertex[];
};

layout (std430, binding = 2) buffer readonly IndexBuffer { // Maybe... Without the index bffer there would be a lessened memory overhead, but let's ignore this for now...
	uint index[];
};

layout(binding = 3) uniform Light {
	vec3 position;
	vec3 color;
	vec3 ambientStrength;
} light;

#define INFINITY (1.0f / 0.0f)

#define PROJECT(vector, axis) (axis*(dot(vector, axis) / dot(axis, axis)))

vec3 getMasses(in Triangle triangle, in vec3 point) {
	vec3 ab = triangle.b.position - triangle.a.position;
	vec3 bc = triangle.c.position - triangle.b.position;
	vec3 ae = ab - PROJECT(ab, bc);

	vec3 ax = (point - triangle.a.position);
	vec3 ad = PROJECT(ax, ae);
	if (ae.x < 0){ ad.x = -ad.x; ae.x = -ae.x; }
	if (ae.y < 0){ ad.y = -ad.y; ae.y = -ae.y; }
	if (ae.z < 0){ ad.z = -ad.z; ae.z = -ae.z; }
	float div = ad.x + ad.y + ad.z;
	if (div == 0) return vec3(1, 0, 0);
	float g = (ae.x + ae.y + ae.z) / div;

	float t;
	vec3 by = triangle.a.position + ax * g - triangle.b.position;
	if (bc.x < 0){ bc.x = -bc.x; by.x = -by.x; }
	if (bc.y < 0){ bc.y = -bc.y; by.y = -by.y; }
	if (bc.z < 0){ bc.z = -bc.z; by.z = -by.z; }
	div = bc.x + bc.y + bc.z;
	if (div == 0) t = 0;
	else t = (by.x + by.y + by.z) / div;

	float cc = t;
	float bb = (1 - t);
	float aa = (g - 1);

	return(vec3(aa, bb, cc) / (aa + bb + cc));
}

bool triangleContainsVertex(in PosTriangle triangle, in vec3 point) {
	if (point == triangle.a || point == triangle.b || point == triangle.c) return true;
	const vec3 ab = (triangle.b - triangle.a);
	const vec3 bc = (triangle.c - triangle.b);
	const vec3 ca = (triangle.a - triangle.c);
	const vec3 ax = (point - triangle.a);
	const vec3 bx = (point - triangle.b);
	const vec3 cx = (point - triangle.c);
	return(dot(ab, ax) / sqrt(dot(ax, ax)) + 0.00015f >= -dot(ab, ca) / sqrt(dot(ca, ca))
		&& dot(bc, bx) / sqrt(dot(bx, bx)) + 0.00015f >= -dot(bc, ab) / sqrt(dot(ab, ab))
		&& dot(ca, cx) / sqrt(dot(cx, cx)) + 0.00015f >= -dot(ca, bc) / sqrt(dot(bc, bc)));
}

bool castRayOnTriangle(in Ray ray, in PosTriangle triangle, out float distance, out vec3 hitPoint) {
	const vec3 normal = cross((triangle.b - triangle.a), (triangle.c - triangle.a));
	const float deltaProjection = dot((triangle.a - ray.origin), normal);
	if (deltaProjection > 0.0f) return false;
	const float dirProjection = dot(ray.direction, normal);
	if ((deltaProjection * dirProjection) <= 0) return false;
	const float dist = deltaProjection / dirProjection;
	const vec3 hitVert = ray.origin + ray.direction * dist;
	if (triangleContainsVertex(triangle, hitVert)){
		distance = dist;
		hitPoint = hitVert;
		return true;
	}
	else return false;
}

bool raycast(in Ray ray, out Triangle triangle, out float distance, out vec3 hitPoint) {
	float dist = INFINITY;
	uint triangleId = 0;
	vec3 point = vec3(0.0f, 0.0f, 0.0f);
	for (uint i = 0; i < vertex.length(); i += 3) {
		PosTriangle tri;
		tri.a = vertex[index[i]].position;
		tri.b = vertex[index[i + 1]].position;
		tri.c = vertex[index[i + 2]].position;
		float dst;
		vec3 pnt;
		if (castRayOnTriangle(ray, tri, dst, pnt))
			if (dst < dist) {
				dist = dst;
				point = pnt;
				triangleId = i;
			}
	}
	if (isinf(dist)) return false;
	else {
		distance = dist;
		triangle.a = vertex[index[triangleId]];
		triangle.b = vertex[index[triangleId + 1]];
		triangle.c = vertex[index[triangleId + 2]];
		hitPoint = point;
		return true;
	}
}

vec4 shade(in vec3 worldPos, in vec3 fragNormal, in vec3 pixelColor) {
	vec3 deltaPos = (light.position - worldPos);
	float sqrDistance = dot(deltaPos, deltaPos);
	vec3 color = (light.color / sqrDistance);
	vec3 dirToLight = (deltaPos / sqrt(sqrDistance));
	float diffuse = dot(dirToLight, fragNormal);
	if (diffuse <= 0) diffuse = 0.0f;
	
	// Shadows:
	{
		Ray ray;
		ray.origin = light.position;
		ray.direction = -dirToLight;
		Triangle triangle;
		float distance;
		vec3 hitPoint;
		if (raycast(ray, triangle, distance, hitPoint))
			if ((distance * distance) < (sqrDistance - 0.025f))
				diffuse = 0.0f;
	}
	vec3 conserved = (light.ambientStrength + diffuse); 
	return vec4(pixelColor * color * conserved, 1.0f);
}

layout(location = 0) in vec3 rayOrigin;
layout(location = 1) in vec3 rawRayDirection;

layout(location = 0) out vec4 outColor;

void main() {
	Ray ray;
	ray.origin = rayOrigin;
	ray.direction = normalize(rawRayDirection);
	vec3 vectorToCenter = normalize(-rayOrigin);
	
	Triangle triangle;
	float distance;
	vec3 hitPoint;

	if (raycast(ray, triangle, distance, hitPoint)) {
		vec3 masses = getMasses(triangle, hitPoint);
		vec3 fragNormal = ((triangle.a.normal * masses.x) + (triangle.b.normal * masses.y) + (triangle.c.normal * masses.z));
		vec3 pixelColor = ((triangle.a.color * masses.x) + (triangle.b.color * masses.y) + (triangle.c.color * masses.z));
		outColor = shade(hitPoint, fragNormal, pixelColor);
	}
	else {
		// This is basically not a thing we should even be doing, but some bluish background makes RT mode distinct... 
		// (I had a bit of fun when developing and since this causes no harm, why not leave it be..)
		float centerCloseness = dot(ray.direction, vectorToCenter);
		centerCloseness = pow(centerCloseness, 16);
		outColor = vec4(centerCloseness, centerCloseness, 1.0f, 1.0f);
	}
}
