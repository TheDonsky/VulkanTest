#include "RasterizedMesh.h"


namespace Test {
	RasterizedMesh::RasterizedMesh(
		const std::shared_ptr<Mesh>& mesh, 
		const std::shared_ptr<VPTransform>& transform, const std::shared_ptr<PointLight>& light,
		void(*logFn)(const char*))
		: m_mesh(mesh)
		, m_vpTransform(transform), m_vpTransformBuffer(m_mesh->device(), m_vpTransform.operator->(), logFn)
		, m_light(light), m_lightBuffer(m_mesh->device(), m_light.operator->(), logFn) {
		{
			m_vpTransformBufferInfo = {};
			m_vpTransformBufferInfo.buffer = m_vpTransformBuffer.stagingBuffer();
			m_vpTransformBufferInfo.offset = 0;
			m_vpTransformBufferInfo.range = sizeof(VPTransform);
		}
		{
			m_lightBufferInfo = {};
			m_lightBufferInfo.buffer = m_lightBuffer.stagingBuffer();
			m_lightBufferInfo.offset = 0;
			m_lightBufferInfo.range = sizeof(PointLight);
		}
	}

	RasterizedMesh::~RasterizedMesh() { }

	bool RasterizedMesh::initialized() {
		return m_vpTransformBuffer.stagingBuffer() != VK_NULL_HANDLE && m_lightBuffer.stagingBuffer() != VK_NULL_HANDLE;
	}

	const char* RasterizedMesh::vertexShader() {
		static const char SHADER[] = "__Test__/Shaders/RasterizedDiffuseVert.spv";
		return SHADER;
	}

	const char* RasterizedMesh::fragmentShader() {
		static const char SHADER[] = "__Test__/Shaders/RasterizedDiffuseFrag.spv";
		return SHADER;
	}

	VkPipelineVertexInputStateCreateInfo RasterizedMesh::vertexInputInfo() {
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &PNCVertex::bindingDescription();
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(PNCVertex::attributeDescription().size());
		vertexInputInfo.pVertexAttributeDescriptions = PNCVertex::attributeDescription().data();
		return vertexInputInfo;
	}

	uint32_t RasterizedMesh::numVertices() {
		return m_mesh->numVertices();
	}

	VkBuffer RasterizedMesh::vertexBuffer() {
		return m_mesh->vertexBuffer();
	}

	uint32_t RasterizedMesh::numIndices() {
		return m_mesh->numIndices();
	}

	VkBuffer RasterizedMesh::indexBuffer() {
		return m_mesh->indexBuffer();
	}

	uint32_t RasterizedMesh::numLayoutBindings() {
		return 2;
	}

	VkDescriptorSetLayoutBinding RasterizedMesh::layoutBinding(uint32_t index) {
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = index;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		if (index == 0) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		}
		else if (index == 1) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		return binding;
	}

	VkWriteDescriptorSet RasterizedMesh::descriptorBinding(uint32_t index) {
		VkWriteDescriptorSet binding = {};
		binding.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		binding.dstSet = VK_NULL_HANDLE;
		binding.dstBinding = index;
		binding.dstArrayElement = 0;
		binding.descriptorCount = 1;
		if (index == 0) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.pBufferInfo = &m_vpTransformBufferInfo;
		}
		else if (index == 1) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.pBufferInfo = &m_lightBufferInfo;
		}
		return binding;
	}

	void RasterizedMesh::updateResources() {
		m_vpTransformBuffer.setContent(m_vpTransform.operator->());
		m_lightBuffer.setContent(m_light.operator->());
	}
}
