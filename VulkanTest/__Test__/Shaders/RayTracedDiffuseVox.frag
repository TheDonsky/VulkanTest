#version 450
#extension GL_ARB_separate_shader_objects : enable

// Enable this to display voxel grid as well:
//#define SHOW_DEBUG_VOXELS

/** ########################################################################################################### */
/** TYPE DEFINITIONS: */
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

struct AABB {
	vec3 start;
	vec3 end;
};

struct VoxelEntry {
	uint triangle;
	uint next;
};





/** ########################################################################################################### */
/** INPUTS: */
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

layout(binding = 4) uniform GridSettings {
	vec3 gridStart;
	vec3 gridEnd;
	uvec3 numDivisions;
} voxelSettings;

layout(std430, binding = 5) buffer readonly VoxelGridData {
	uint voxelGrid[];
};

layout(std430, binding = 6) buffer readonly VoxelEntryData {
	VoxelEntry voxelEntry[];
};

layout(location = 0) in vec3 rayOrigin;
layout(location = 1) in vec3 rawRayDirection;

layout(location = 0) out vec4 outColor;





/** ########################################################################################################### */
/** TRIANGLE: */
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





/** ########################################################################################################### */
/** RAYCAST: */
#define INFINITY (1.0f / 0.0f)
#define NO_ENTRY (~(uint(0)))

bool pointInAABB(in vec3 point, in AABB aabb) {
	return 
		(point.x >= aabb.start.x && point.x <= aabb.end.x) &&
		(point.y >= aabb.start.y && point.y <= aabb.end.y) &&
		(point.z >= aabb.start.z && point.z <= aabb.end.z);
}

vec3 cellSize() {
	return ((voxelSettings.gridEnd - voxelSettings.gridStart) / voxelSettings.numDivisions);
}

bool findFirstCell(in Ray ray, out uvec3 cellId, out vec3 point) {
	AABB fullBox;
	fullBox.start = voxelSettings.gridStart + 0.000001f;
	fullBox.end = voxelSettings.gridEnd - 0.000001f;
	
	vec3 invDir = 1.0f / ray.direction;
	float ds = (voxelSettings.gridStart.x - ray.origin.x) * invDir.x;
	float de = (voxelSettings.gridEnd.x - ray.origin.x) * invDir.x;
	float mn = min(ds, de), mx = max(ds, de);
	ds = (voxelSettings.gridStart.y - ray.origin.y) * invDir.y;
	de = (voxelSettings.gridEnd.y - ray.origin.y) * invDir.y;
	mn = max(mn, min(ds, de));
	mx = min(mx, max(ds, de));
	ds = (voxelSettings.gridStart.z - ray.origin.z) * invDir.z;
	de = (voxelSettings.gridEnd.z - ray.origin.z) * invDir.z;
	mn = max(mn, min(ds, de));
	mx = min(mx, max(ds, de));
	if (mn > mx + 0.0001f) return false;
	else if (mn < 0.0f) mn = 0.0f;
	point = (ray.origin + (mn * ray.direction));
	// To make sure, point is exactly inside the box and avoid random erros caused by floating point calculations:
	{
		if (point.x < fullBox.start.x) point.x = fullBox.start.x;
		if (point.y < fullBox.start.y) point.y = fullBox.start.y;
		if (point.z < fullBox.start.z) point.z = fullBox.start.z;

		if (point.x > fullBox.end.x) point.x = fullBox.end.x;
		if (point.y > fullBox.end.y) point.y = fullBox.end.y;
		if (point.z > fullBox.end.z) point.z = fullBox.end.z;
	}
	cellId = uvec3((point - voxelSettings.gridStart) / cellSize());
	return true;
}

bool findNextCell(inout Ray invRay, in vec3 direction, in vec3 cellSz, inout AABB cell, inout uvec3 cellId) {
	ivec3 indexDelta = ivec3(0, 0, 0);
	float minDist = INFINITY;
	
	const vec3 startTime = (cell.start - invRay.origin) * invRay.direction;
	const vec3 endTime = (cell.end - invRay.origin) * invRay.direction;

	if (invRay.direction.x > 0) {
		if (minDist > endTime.x && cellId.x < (voxelSettings.numDivisions.x - 1)) {
			indexDelta = ivec3(1, 0, 0);
			minDist = endTime.x;
		}
	}
	else if (invRay.direction.x < 0) {
		if (minDist > startTime.x && cellId.x > 0) {
			indexDelta = ivec3(-1, 0, 0);
			minDist = startTime.x;
		}
	}

	if (invRay.direction.y > 0) {
		if (minDist > endTime.y && cellId.y < (voxelSettings.numDivisions.y - 1)) {
			indexDelta = ivec3(0, 1, 0);
			minDist = endTime.y;
		}
	}
	else if (invRay.direction.y < 0) {
		if (minDist > startTime.y && cellId.y > 0) {
			indexDelta = ivec3(0, -1, 0);
			minDist = startTime.y;
		}
	}

	if (invRay.direction.z > 0) {
		if (minDist > endTime.z && cellId.z < (voxelSettings.numDivisions.z - 1)) {
			indexDelta = ivec3(0, 0, 1);
			minDist = endTime.z;
		}
	}
	else if (invRay.direction.z < 0) {
		if (minDist > startTime.z && cellId.z > 0) {
			indexDelta = ivec3(0, 0, -1);
			minDist = startTime.z;
		}
	}

	if (isinf(minDist) || minDist < 0.0f) return false;

	const vec3 cellDelta = (vec3(indexDelta) * cellSz);
	cell.start += cellDelta;
	cell.end += cellDelta;
	cellId = ivec3(cellId) + indexDelta;
	invRay.origin += direction * minDist;
	return pointInAABB(invRay.origin, cell);
}

bool castInCell(in Ray ray, in uvec3 cellId, in AABB cell, out Triangle triangle, out float distance, out vec3 hitPoint) {
	float dist = INFINITY;
	uint triangleId = 0;
	vec3 point = vec3(0.0f, 0.0f, 0.0f);
	uint entryId = voxelGrid[((voxelSettings.numDivisions.x * ((cellId.z * voxelSettings.numDivisions.y) + cellId.y)) + cellId.x)];
	while (entryId != NO_ENTRY) {
#ifdef SHOW_DEBUG_VOXELS
		outColor.r = min(outColor.r + 0.1f, 1.0f);
#endif
		const VoxelEntry entry = voxelEntry[entryId];
		PosTriangle tri;
		tri.a = vertex[index[entry.triangle]].position;
		tri.b = vertex[index[entry.triangle + 1]].position;
		tri.c = vertex[index[entry.triangle + 2]].position;
		float dst;
		vec3 pnt;
		if (castRayOnTriangle(ray, tri, dst, pnt))
			if (dst < dist && pointInAABB(pnt, cell)) {
				dist = dst;
				point = pnt;
				triangleId = entry.triangle;
			}
		entryId = entry.next;
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

bool raycast(in Ray ray, out Triangle triangle, out float distance, out vec3 hitPoint) {
	uvec3 cellId;
	vec3 point;
	if (!findFirstCell(ray, cellId, point)) return false;
	Ray invRay;
	{
		invRay.origin = point;
		invRay.direction = 1.0f / ray.direction;
	}
	const vec3 cellSz = cellSize();
	AABB cell;
	{
		cell.start = ((cellSz * vec3(cellId)) + voxelSettings.gridStart);
		cell.end = (cell.start + cellSz + 0.000025f);
		cell.start -= 0.000025f;
	}
	while (true) {
		if (castInCell(ray, cellId, cell, triangle, distance, hitPoint)) return true;
		else if (!findNextCell(invRay, ray.direction, cellSz, cell, cellId)) return false;
#ifdef SHOW_DEBUG_VOXELS
		outColor.g += 1.0f / float(voxelSettings.numDivisions.x + voxelSettings.numDivisions.y + voxelSettings.numDivisions.z);
#endif
	}
}





/** ########################################################################################################### */
/** SHADING: */
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





/** ########################################################################################################### */
/** ENTRY POINT: */
void main() {
#ifdef SHOW_DEBUG_VOXELS
	outColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
#endif

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
#ifndef SHOW_DEBUG_VOXELS
	else {
		float centerCloseness = dot(ray.direction, vectorToCenter);
		centerCloseness = pow(centerCloseness, 16);
		outColor = vec4(1.0f, centerCloseness, centerCloseness, 1.0f);
	}
#endif
}
