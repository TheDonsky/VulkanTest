#include "RayTracedMesh.h"

namespace {
	static const std::vector<glm::vec3> VERTEX_BUFFER = {
		{-1.0f, -1.0f, 0.5f}, {1.0f, -1.0f, 0.5f},
		{-1.0f, 1.0f, 0.5f}, {1.0f, 1.0f, 0.5f}
	};

	static const std::vector<uint32_t> INDEX_BUFFER = {
		0, 2, 1,
		2, 3, 1
	};

	inline static VkVertexInputBindingDescription VertexBindingDescription() {
		VkVertexInputBindingDescription desc = {};
		desc.binding = 0;
		desc.stride = sizeof(glm::vec3);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}
	static const VkVertexInputBindingDescription BINDING_DESCRIPTION = VertexBindingDescription();

	inline static VkVertexInputAttributeDescription VertexAttributeDescription() {
		VkVertexInputAttributeDescription desc = {};
		{
			desc.binding = 0;
			desc.location = 0;
			desc.format = VK_FORMAT_R32G32B32_SFLOAT;
			desc.offset = 0;
		}
		return desc;
	}
	static const VkVertexInputAttributeDescription ATTRIBUTE_DESCRIPTION = VertexAttributeDescription();
}

namespace Test {
	RayTracedMesh::RayTracedMesh(const std::shared_ptr<Mesh>& mesh,
		const std::shared_ptr<VPTransform>& transform, const std::shared_ptr<PointLight>& light,
		void(*logFn)(const char*)) 
		: m_mesh(mesh), m_vpTransform(transform), m_light(light)
		, m_vertexBuffer(m_mesh->device(), static_cast<uint32_t>(VERTEX_BUFFER.size()), VERTEX_BUFFER.data(), logFn)
		, m_indexBuffer(m_mesh->device(), static_cast<uint32_t>(INDEX_BUFFER.size()), INDEX_BUFFER.data(), logFn)
		, m_inverseTransformBuffer(m_mesh->device(), nullptr, logFn)
		, m_lightBuffer(m_mesh->device(), m_light.operator->(), logFn) {
		{
			m_vpTransformBufferInfo = {};
			m_vpTransformBufferInfo.buffer = m_inverseTransformBuffer.stagingBuffer();
			m_vpTransformBufferInfo.offset = 0;
			m_vpTransformBufferInfo.range = sizeof(VPTransform);
		}
		{
			m_vertexBufferInfo = {};
			m_vertexBufferInfo.buffer = m_mesh->vertexBuffer();
			m_vertexBufferInfo.offset = 0;
			m_vertexBufferInfo.range = VK_WHOLE_SIZE;
		}
		{
			m_indexBufferInfo = {};
			m_indexBufferInfo.buffer = m_mesh->indexBuffer();
			m_indexBufferInfo.offset = 0;
			m_indexBufferInfo.range = VK_WHOLE_SIZE;
		}
		{
			m_lightBufferInfo = {};
			m_lightBufferInfo.buffer = m_lightBuffer.stagingBuffer();
			m_lightBufferInfo.offset = 0;
			m_lightBufferInfo.range = sizeof(PointLight);
		}
	}

	RayTracedMesh::~RayTracedMesh() { }

	bool RayTracedMesh::initialized() {
		return (m_vertexBuffer.buffer() != VK_NULL_HANDLE && m_indexBuffer.buffer() != VK_NULL_HANDLE
			&& m_inverseTransformBuffer.stagingBuffer() != VK_NULL_HANDLE
			&& m_lightBuffer.stagingBuffer() != VK_NULL_HANDLE);
	}

	const char* RayTracedMesh::vertexShader() {
		static const char SHADER[] = "__Test__/Shaders/RayTracedDiffuseVert.spv";
		return SHADER;
	}

	const char* RayTracedMesh::fragmentShader() {
		static const char SHADER[] = "__Test__/Shaders//RayTracedDiffuseFrag.spv";
		return SHADER;
	}

	VkPipelineVertexInputStateCreateInfo RayTracedMesh::vertexInputInfo() {
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &BINDING_DESCRIPTION;
		vertexInputInfo.vertexAttributeDescriptionCount = 1;
		vertexInputInfo.pVertexAttributeDescriptions = &ATTRIBUTE_DESCRIPTION;
		return vertexInputInfo;
	}

	uint32_t RayTracedMesh::numVertices() {
		return m_vertexBuffer.size();
	}

	VkBuffer RayTracedMesh::vertexBuffer() {
		return m_vertexBuffer.buffer();
	}

	uint32_t RayTracedMesh::numIndices() {
		return m_indexBuffer.size();
	}

	VkBuffer RayTracedMesh::indexBuffer() {
		return m_indexBuffer.buffer();
	}

	uint32_t RayTracedMesh::numLayoutBindings() {
		return 4;
	}

	VkDescriptorSetLayoutBinding RayTracedMesh::layoutBinding(uint32_t index) {
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = index;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		if (index == 0) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		}
		else if (index == 1 || index == 2) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		else if (index == 3) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		return binding;
	}

	VkWriteDescriptorSet RayTracedMesh::descriptorBinding(uint32_t index) {
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
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			binding.pBufferInfo = &m_vertexBufferInfo;
		}
		else if (index == 2) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			binding.pBufferInfo = &m_indexBufferInfo;
		}
		else if (index == 3) {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.pBufferInfo = &m_lightBufferInfo;
		}
		return binding;
	}

	void RayTracedMesh::updateResources() {
		VPTransform inverseTransform;
		inverseTransform.view = glm::inverse(m_vpTransform->view);
		inverseTransform.projection = glm::inverse(m_vpTransform->projection);
		m_inverseTransformBuffer.setContent(&inverseTransform);
		m_lightBuffer.setContent(m_light.operator->());
	}
}
