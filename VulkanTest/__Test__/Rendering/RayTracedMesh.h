#pragma once
#include "RenderObject.h"
#include "../Objects/Mesh.h"
#include "../Objects/VoxelGrid.h"

namespace Test {
	/**
	 * Object, capable of ray-traced rendering.
	 * For additional documentation of the individual functions, read RenderObject.h
	 * As a side note for those, wondering how we managed to fit a raytracer into an interface that was clearly meant for rasterisation,
	 * here's the hacky way I decided to go with:
	 *	0. We feed screen corners as vertex shader input, alongside the index buffer that causes the rasterizer to fill entire screen with fragments;
	 *	1. Vertex shader passes vertex buffer to gl_Position without modification, but also applies an inverse transformation to it to get incomming ray direction for all corners;
	 *	2. Fragment shaders get unnormalized rays as input, as well as storage buffers for vertex and index data of the geometry;
	 *	3. Ray is cast into the void, hitting some triangle, that's then shaded (of course, we are casting an additional ray to understand, if the surface can even be reached);
	 *	4. We write some other color wherever we missed the geometry altogather (actually, We're filling with some color tinted gradient, that I initially used to make sure the fragments were casting rays in the right direction and than I decided it looked cool);
	 *	5. After all this hard work, we have a ray-traced image and a terrible performance, when we are not using any accelerating data structures and/or hardware solutons (VoxelGrid helps, really).
	 */
	class RayTracedMesh : public IRenderObject {
	public:
		/**
		Creates a ray-tracer.
		@param mesh Scene geometry.
		@param transform Reference to the View-Projection transformation.
		@param light Information about scene lighing.
		@param voxelGrid Voxel grid for acceleration.
		@param logFn Logging function for error reporting (optional).
		*/
		RayTracedMesh(const std::shared_ptr<Mesh>& mesh,
			const std::shared_ptr<VPTransform>& transform, const std::shared_ptr<PointLight>& light,
			const std::shared_ptr<VoxelGrid>& voxelGrid = nullptr,
			void(*logFn)(const char*) = nullptr);

		/** Destructor */
		virtual ~RayTracedMesh();

		virtual bool initialized() override;

		virtual const char* vertexShader() override;

		virtual const char* fragmentShader() override;

		virtual VkPipelineVertexInputStateCreateInfo vertexInputInfo() override;

		virtual uint32_t numVertices() override;

		virtual VkBuffer vertexBuffer() override;

		virtual uint32_t numIndices() override;

		virtual VkBuffer indexBuffer() override;

		virtual uint32_t numLayoutBindings() override;

		virtual VkDescriptorSetLayoutBinding layoutBinding(uint32_t index) override;

		virtual VkWriteDescriptorSet descriptorBinding(uint32_t index) override;

		virtual void updateResources() override;


	private:
		const std::shared_ptr<Mesh> m_mesh;
		const std::shared_ptr<VPTransform> m_vpTransform;
		const std::shared_ptr<PointLight> m_light;
		const std::shared_ptr<VoxelGrid>& m_voxelGrid;

		VertexBuffer<glm::vec3> m_vertexBuffer;
		IndexBuffer m_indexBuffer;
		ConstantBuffer<VPTransform> m_inverseTransformBuffer;
		ConstantBuffer<PointLight> m_lightBuffer;

		VkDescriptorBufferInfo m_vpTransformBufferInfo;
		VkDescriptorBufferInfo m_vertexBufferInfo;
		VkDescriptorBufferInfo m_indexBufferInfo;
		VkDescriptorBufferInfo m_lightBufferInfo;

		VkDescriptorBufferInfo m_voxelSettingsInfo;
		VkDescriptorBufferInfo m_voxelGridInfo;
		VkDescriptorBufferInfo m_voxelEntryInfo;
	};
}

