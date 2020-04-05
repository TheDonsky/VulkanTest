#include "VoxelGrid.h"
#include <algorithm>

namespace {
	struct Triangle {
		glm::vec3 a, b, c;

		inline Triangle(const glm::vec3& pa = {}, const glm::vec3& pb = {}, const glm::vec3& pc = {}) : a(pa), b(pb), c(pc) {}

		inline void sortByMases(float am, float bm, float cm) {
			if (am > bm) {
				if (cm > am) std::swap(a, b); // b a c
				else {
					if (bm > cm) std::swap(a, c); // c b a
					else { glm::vec3 tmp = a; a = b; b = c; c = tmp; } // b c a
				}
			}
			else {
				if (am > cm) { glm::vec3 tmp = a; a = c; c = b; b = tmp; } // c a b
				else if (bm > cm) std::swap(b, c); // a c b
			}
		}
	};

	class AABB {
	public:
		glm::vec3 start, end;

		inline bool intersects(const Triangle& t)const;


	private:
		inline bool intersectsTri(unsigned int dimm, Triangle t)const;
		inline bool intersectsTri(unsigned int dimm, const Triangle& t, float av, float bv, float cv, float s, float e)const;
	};


	inline bool AABB::intersects(const Triangle& t)const {
		return intersectsTri(0, t);
	}

	inline bool AABB::intersectsTri(unsigned int dimm, Triangle t)const {
		if (dimm == 0) {
			t.sortByMases(t.a.z, t.b.z, t.c.z);
			return intersectsTri(dimm + 1, t, t.a.z, t.b.z, t.c.z, start.z, end.z);
		}
		else if (dimm == 1) {
			t.sortByMases(t.a.x, t.b.x, t.c.x);
			return intersectsTri(dimm + 1, t, t.a.x, t.b.x, t.c.x, start.x, end.x);
		}
		else if (dimm == 2) {
			t.sortByMases(t.a.y, t.b.y, t.c.y);
			return intersectsTri(dimm + 1, t, t.a.y, t.b.y, t.c.y, start.y, end.y);
		}
		else return true;
	}
#define CROSS_POINT(name, from, to, fromV, toV, barrier) glm::vec3 name = from + (to - from) * ((barrier - fromV) / (toV - fromV))
	inline bool AABB::intersectsTri(unsigned int dimm, const Triangle& t, float av, float bv, float cv, float s, float e)const {
		if (cv < s) return(false); // a b c | | (1)
		if (av > e) return(false); // | | a b c (10)
		if (av <= s) {
			CROSS_POINT(asc, t.a, t.c, av, cv, s);
			if (bv <= s) {
				CROSS_POINT(bsc, t.b, t.c, bv, cv, s);
				if (cv <= e) return(intersectsTri(dimm,  Triangle(asc, bsc, t.c))); // a b | c | (2)
				else { // a b | | c (3)
					CROSS_POINT(bec, t.b, t.c, bv, cv, e);
					if (intersectsTri(dimm,  Triangle(bsc, bec, asc))) return(true);
					CROSS_POINT(aec, t.a, t.c, av, cv, e);
					return(intersectsTri(dimm,  Triangle(asc, bec, aec)));
				}
			}
			else if (bv <= e) {
				if (cv <= e) { // a | b c | (4)
					if (intersectsTri(dimm,  Triangle(asc, t.b, t.c))) return(true);
					CROSS_POINT(asb, t.a, t.b, av, bv, s);
					return(intersectsTri(dimm,  Triangle(asc, asb, t.b)));
				}
				else { // a | b | c (5)
					CROSS_POINT(asb, t.a, t.b, av, bv, s);
					CROSS_POINT(bec, t.b, t.c, bv, cv, e);
					if (intersectsTri(dimm,  Triangle(asb, t.b, bec))) return(true);
					if (intersectsTri(dimm,  Triangle(asc, asb, bec))) return(true);
					CROSS_POINT(aec, t.a, t.c, av, cv, e);
					return(intersectsTri(dimm,  Triangle(asc, bec, aec)));
				}
			}
			else { // a | | b c (6)
				CROSS_POINT(asb, t.a, t.b, av, bv, s);
				CROSS_POINT(aeb, t.a, t.b, av, bv, e);
				if (intersectsTri(dimm,  Triangle(asc, asb, aeb))) return(true);
				CROSS_POINT(aec, t.a, t.c, av, cv, e);
				return(intersectsTri(dimm,  Triangle(asc, aeb, aec)));
			}
		}
		else {
			if (cv <= e) return(intersectsTri(dimm,  t)); // | a b c | (7)
			else {
				CROSS_POINT(aec, t.a, t.c, av, cv, e);
				if (bv <= e) { // | a b | c (8)
					CROSS_POINT(bec, t.b, t.c, bv, cv, e);
					if (intersectsTri(dimm,  Triangle(t.a, t.b, bec))) return(true);
					return(intersectsTri(dimm,  Triangle(t.a, aec, bec)));
				}
				else { // | a | b c (9)
					CROSS_POINT(aeb, t.a, t.b, av, bv, e);
					return(intersectsTri(dimm,  Triangle(t.a, aeb, aec)));
				}
			}
		}
	}
#undef CROSS_POINT

#define NO_VOXEL_ENTRY (~((Test::VoxelGrid::VoxelData::VoxelEntryId)0))
}

namespace Test {
	VoxelGrid::VoxelData::VoxelData(const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, const glm::uvec3& numDivisions) {
		{
			glm::vec3 first = (verts.size() <= 0 ? glm::vec3{ 0.0f, 0.0f, 0.0f } : verts[0].position);
			settings.gridStart = first;
			settings.gridEnd = first;
			settings.numDivisions = numDivisions;
		}
		for (size_t i = 0; i < verts.size(); i++) {
			const glm::vec3 pos = verts[i].position;
			if (settings.gridStart.x > pos.x) settings.gridStart.x = pos.x;
			if (settings.gridStart.y > pos.y) settings.gridStart.y = pos.y;
			if (settings.gridStart.z > pos.z) settings.gridStart.z = pos.z;
			if (settings.gridEnd.x < pos.x) settings.gridEnd.x = pos.x;
			if (settings.gridEnd.y < pos.y) settings.gridEnd.y = pos.y;
			if (settings.gridEnd.z < pos.z) settings.gridEnd.z = pos.z;
		}
		{
			settings.gridStart -= FLT_EPSILON * 32;
			settings.gridEnd += FLT_EPSILON * 32;
		}
		voxels.resize(static_cast<size_t>(settings.numDivisions.x) * settings.numDivisions.y * settings.numDivisions.z, NO_VOXEL_ENTRY);
		const glm::vec3 cellSize = (settings.gridEnd - settings.gridStart) / (glm::vec3)settings.numDivisions;
		for (size_t i = 2; i < indexBuffer.size(); i += 3) {
			const Triangle triangle(verts[indexBuffer[i - 2]].position, verts[indexBuffer[i - 1]].position, verts[indexBuffer[i]].position);
			glm::uvec3 minIndex = {
				static_cast<uint32_t>((std::min(std::min(triangle.a.x, triangle.b.x), triangle.c.x) - settings.gridStart.x) / cellSize.x),
				static_cast<uint32_t>((std::min(std::min(triangle.a.y, triangle.b.y), triangle.c.y) - settings.gridStart.y) / cellSize.y),
				static_cast<uint32_t>((std::min(std::min(triangle.a.z, triangle.b.z), triangle.c.z) - settings.gridStart.z) / cellSize.z)
			};
			glm::uvec3 maxIndex = {
				static_cast<uint32_t>((std::max(std::max(triangle.a.x, triangle.b.x), triangle.c.x) - settings.gridStart.x) / cellSize.x),
				static_cast<uint32_t>((std::max(std::max(triangle.a.y, triangle.b.y), triangle.c.y) - settings.gridStart.y) / cellSize.y),
				static_cast<uint32_t>((std::max(std::max(triangle.a.z, triangle.b.z), triangle.c.z) - settings.gridStart.z) / cellSize.z)
			};
			for (uint32_t x = minIndex.x; x <= maxIndex.x; x++)
				for (uint32_t y = minIndex.y; y <= maxIndex.y; y++)
					for (uint32_t z = minIndex.z; z <= maxIndex.z; z++) {
						AABB cell;
						{
							cell.start = settings.gridStart + cellSize * glm::vec3(x, y, z);
							cell.end = cell.start + cellSize + FLT_EPSILON;
							cell.start -= FLT_EPSILON;
						}
						if (cell.intersects(triangle)) {
							size_t voxelId = ((settings.numDivisions.x * ((static_cast<size_t>(z) * settings.numDivisions.y) + y)) + x);
							VoxelEntry entry;
							entry.triangle = static_cast<uint32_t>(i - 2);
							entry.next = voxels[voxelId];
							voxels[voxelId] = static_cast<VoxelEntryId>(voxelEntries.size());
							voxelEntries.push_back(entry);
						}
					}
		}
	}

	VoxelGrid::VoxelGrid(const std::shared_ptr<GraphicsDevice>& device, const VoxelData& data, void(*logFn)(const char*)) 
		: settings(device, &data.settings, logFn)
		, voxels(device, static_cast<uint32_t>(data.voxels.size()), data.voxels.data(), logFn)
		, entries(device, static_cast<uint32_t>(data.voxelEntries.size()), data.voxelEntries.data(), logFn) { }

	VoxelGrid::VoxelGrid(const std::shared_ptr<GraphicsDevice>& device, const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, const glm::uvec3& numDivisions, void(*logFn)(const char*))
		: VoxelGrid(device, VoxelData(verts, indexBuffer, numDivisions), logFn) { }

	bool VoxelGrid::initialized()const {
		return (settings.stagingBuffer() != VK_NULL_HANDLE && voxels.buffer() != VK_NULL_HANDLE && entries.buffer() != VK_NULL_HANDLE);
	}
}
