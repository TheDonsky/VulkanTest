#pragma once
#include "RenderObject.h"
#include "../Objects/Mesh.h"

namespace Test {
	/**
	 * Object, capable of rasterized rendering.
	 * For additional documentation of the individual functions, read RenderObject.h
	 */
	class RasterizedMesh : public IRenderObject {
	public:
		/**
		Creates a rasterizer.
		@param mesh Scene geometry.
		@param transform Reference to the View-Projection transformation.
		@param light Information about scene lighing.
		@param logFn Logging function for error reporting (optional).
		*/
		RasterizedMesh(const std::shared_ptr<Mesh>& mesh, 
			const std::shared_ptr<VPTransform>& transform, const std::shared_ptr<PointLight>& light, 
			void(*logFn)(const char*) = nullptr);

		/** Destructor */
		virtual ~RasterizedMesh();

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
		ConstantBuffer<VPTransform> m_vpTransformBuffer;
		const std::shared_ptr<PointLight> m_light;
		ConstantBuffer<PointLight> m_lightBuffer;

		VkDescriptorBufferInfo m_vpTransformBufferInfo;
		VkDescriptorBufferInfo m_lightBufferInfo;
	};

}
