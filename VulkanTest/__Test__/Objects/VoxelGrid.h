#pragma once
#include "Buffers.h"
#include "Inputs.h"

namespace Test {
	struct VoxelGrid {
		struct VoxelData {
			struct GridSettings {
				alignas(16) glm::vec3 gridStart;
				alignas(16) glm::vec3 gridEnd;
				alignas(16) glm::uvec3 numDivisions;
			};

			typedef uint32_t VoxelEntryId;

			struct VoxelEntry {
				uint32_t triangle;
				VoxelEntryId next;
			};

			GridSettings settings;

			std::vector<VoxelEntryId> voxels;

			std::vector<VoxelEntry> voxelEntries;

			VoxelData(const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, const glm::uvec3& numDivisions = {32, 32, 32});
		};

		
		VoxelGrid(const std::shared_ptr<GraphicsDevice>& device, const VoxelData& data, void(*logFn)(const char*) = nullptr);

		VoxelGrid(const std::shared_ptr<GraphicsDevice>& device, const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, const glm::uvec3& numDivisions = {32, 32, 32}, void(*logFn)(const char*) = nullptr);

		bool initialized()const;

		const ConstantBuffer<VoxelData::GridSettings> settings;

		const Buffer<VoxelData::VoxelEntryId, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> voxels;

		const Buffer<VoxelData::VoxelEntry, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> entries;
	};
}
