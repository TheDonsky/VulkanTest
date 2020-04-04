#pragma once
#include "Buffers.h"
#include "Inputs.h"

namespace Test {
	/**
	 * Represents a voxel grid for arbitrary geometry.
	 * This, alongside with corresponding mesh can be used for faster ray tracing.
	 */
	struct VoxelGrid {
		/**
		 * CPU "Clone" of the voxel grid, containing settings, voxels and entries.
		 */
		struct VoxelData {
			/**
			 * Basic description of a voxel grid.
			 */
			struct GridSettings {
				// Lower left nearest corner of the entire voxelized volume.
				alignas(16) glm::vec3 gridStart;

				// Upper right furthest corner of the entire voxelized volume.
				alignas(16) glm::vec3 gridEnd;

				// Number of voxel cells per axis.
				alignas(16) glm::uvec3 numDivisions;
			};

			/**
			 * We buld our voxel grid as follows:
			 * We have a flattened grid buffer in memory that holds the index of the first "Entry" for each voxel.
			 * Entry is basically a linked list of triangle indices and nothing else. 
			 * We will be using uints for indexing and here's a type definition for safety.
			 */
			typedef uint32_t VoxelEntryId;

			/**
			 * Represents voxel content.
			 */
			struct VoxelEntry {
				// Triangle inside the voxel.
				uint32_t triangle;

				// Index of the next voxel entry in the linked list of entries.
				VoxelEntryId next;
			};



			// Settings.
			GridSettings settings;

			// Voxel buffer.
			std::vector<VoxelEntryId> voxels;

			// Voxel entry buffer.
			std::vector<VoxelEntry> voxelEntries;

			/**
			Bulds voxel grid.
			@param verts Mesh vertices.
			@param indexBuffer Mesh indices.
			@param numDivisions Number of voxel cells per axis.
			*/
			VoxelData(const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, const glm::uvec3& numDivisions = {32, 32, 32});
		};

		



		/**
		Uploads existing voxel data to GPU.
		@param device Logical device to upload to.
		@param data Baked voxel data.
		@param logFn One function that will help us if anything goes wrong.
		*/
		VoxelGrid(const std::shared_ptr<GraphicsDevice>& device, const VoxelData& data, void(*logFn)(const char*) = nullptr);

		/**
		Builds voxel data and uploads it to GPU.
		@param device Logical device to upload to.
		@param verts Mesh vertices.
		@param indexBuffer Mesh indices.
		@param numDivisions Number of voxel cells per axis.
		@param logFn One function that will help us if anything goes wrong.
		*/
		VoxelGrid(const std::shared_ptr<GraphicsDevice>& device, const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, const glm::uvec3& numDivisions = {32, 32, 32}, void(*logFn)(const char*) = nullptr);

		/**
		Tells if anything went wronf during initialisation.
		@return true, if every single buffer is valid.
		*/
		bool initialized()const;

		

		// Constant buffer, holding information about voxel grid settings (same as VoxelData).
		const ConstantBuffer<VoxelData::GridSettings> settings;

		// Flattened voxel entry indices per voxel (same as VoxelData).
		const Buffer<VoxelData::VoxelEntryId, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> voxels;

		// All the voxel entries (same as VoxelData).
		const Buffer<VoxelData::VoxelEntry, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> entries;
	};
}
