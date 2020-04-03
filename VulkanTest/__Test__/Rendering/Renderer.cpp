#include "Renderer.h"
#include "../Helpers.h"
#include <sstream>

namespace Test {
	Renderer::Renderer(
		const std::shared_ptr<GraphicsDevice>& device, const std::shared_ptr<SwapChain>& swapChain,
		const std::shared_ptr<IRenderObject>& object, void(*logFn)(const char*))
		: m_graphicsDevice(device), m_swapChain(swapChain)
		, m_object(object)
		, m_vertexShaderModule(VK_NULL_HANDLE), m_fragmentShaderModule(VK_NULL_HANDLE)
		, m_descriptorSetLayout(VK_NULL_HANDLE), m_pipelineLayout(VK_NULL_HANDLE)
		, m_graphicsPipeline(VK_NULL_HANDLE), m_descriptorPool(VK_NULL_HANDLE), m_descriptorSet(VK_NULL_HANDLE)
		, m_initialized(false), m_logFn(logFn) {
		
		if (!createShaderModule(m_graphicsDevice->logicalDevice(), m_object->vertexShader(), &m_vertexShaderModule)) {
			std::stringstream stream;
			stream << "[Error] Renderer - Could not create vertex shader module '" << m_object->vertexShader() << "'.";
			log(stream.str().c_str());
		}
		else if (!createShaderModule(m_graphicsDevice->logicalDevice(), m_object->fragmentShader(), &m_fragmentShaderModule)) {
			std::stringstream stream;
			stream << "[Error] Renderer - Could not create fragment shader module '" << m_object->fragmentShader() << "'.";
			log(stream.str().c_str());
		}
		else if (createDescriptorSetLayout())
			createPipelineLayout();

		m_swapChainRecreationListenerId = m_swapChain->addRecreationListener(std::bind(&Renderer::recreateSwapChainDependedObjects, this));
	}

	Renderer::~Renderer() {
		m_swapChain->removeRecreationListener(m_swapChainRecreationListenerId);

		clearSwapChainDependedObjects();

		if (m_pipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(m_graphicsDevice->logicalDevice(), m_pipelineLayout, nullptr);

		if (m_descriptorSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_graphicsDevice->logicalDevice(), m_descriptorSetLayout, nullptr);

		if (m_fragmentShaderModule != VK_NULL_HANDLE)
			vkDestroyShaderModule(m_graphicsDevice->logicalDevice(), m_fragmentShaderModule, nullptr);

		if (m_vertexShaderModule != VK_NULL_HANDLE)
			vkDestroyShaderModule(m_graphicsDevice->logicalDevice(), m_vertexShaderModule, nullptr);
	}

	bool Renderer::initialized() {
		return m_initialized;
	}

	void Renderer::render() {
		size_t imageId;
		VkSemaphore* waitSemaphores;
		VkSemaphore* renderSemaphores;
		if (!m_swapChain->aquireNextImage(imageId, waitSemaphores, renderSemaphores)) return;

		vkQueueWaitIdle(m_graphicsDevice->graphicsQueue()); // Suboptimal, I know..

		m_object->updateResources();

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_commandBuffers[imageId];
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = renderSemaphores;
		}
		if (vkQueueSubmit(m_graphicsDevice->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			log("[Error] Renderer - Failed to submit draw command buffer.");
		}
		m_swapChain->present(imageId);
	}



	void Renderer::log(const char* message)const {
		if (m_logFn != nullptr)
			m_logFn(message);
	}

	bool Renderer::createDescriptorSetLayout() {
		std::vector<VkDescriptorSetLayoutBinding> bindings(m_object->numLayoutBindings());
		for (size_t i = 0; i < bindings.size(); i++)
			bindings[i] = m_object->layoutBinding((uint32_t)i);
		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = static_cast<uint32_t>(bindings.size());
		info.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(m_graphicsDevice->logicalDevice(), &info, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
			m_descriptorSetLayout = VK_NULL_HANDLE;
			log("[Error] Renderer - Failed to create descriptor set layout.");
			return false;
		}
		return true;
	}

	bool Renderer::createPipelineLayout() {
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		{
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
			pipelineLayoutInfo.pushConstantRangeCount = 0;
			pipelineLayoutInfo.pPushConstantRanges = nullptr;
		}
		if (vkCreatePipelineLayout(m_graphicsDevice->logicalDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
			m_pipelineLayout = VK_NULL_HANDLE;
			log("[Error] Renderer - Failed to create pipeline layout.");
			return false;
		}
		return true;
	}

	bool Renderer::createRenderPipeline() {
		VkPipelineShaderStageCreateInfo shaderStages[2];
		VkPipelineShaderStageCreateInfo& vertShaderInfo = shaderStages[0];
		{
			vertShaderInfo = {};
			vertShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderInfo.module = m_vertexShaderModule;
			vertShaderInfo.pName = "main";
		}
		VkPipelineShaderStageCreateInfo& fragShaderInfo = shaderStages[1];
		{
			fragShaderInfo = {};
			fragShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderInfo.module = m_fragmentShaderModule;
			fragShaderInfo.pName = "main";
		}
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = m_object->vertexInputInfo();
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		{
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssembly.primitiveRestartEnable = VK_FALSE;
		}
		VkViewport viewport = {};
		{
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			VkExtent2D size = m_swapChain->size();
			viewport.width = (float)size.width;
			viewport.height = (float)size.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
		}
		VkRect2D scissor = {};
		{
			scissor.offset = { 0, 0 };
			scissor.extent = m_swapChain->size();
		}
		VkPipelineViewportStateCreateInfo viewportState = {};
		{
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;
		}
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		{
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizer.depthBiasEnable = VK_FALSE;
			rasterizer.depthBiasConstantFactor = 0.0f;
			rasterizer.depthBiasClamp = 0.0f;
			rasterizer.depthBiasSlopeFactor = 0.0f;
		}
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		{
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampling.minSampleShading = 1.0f;
			multisampling.pSampleMask = nullptr;
			multisampling.alphaToCoverageEnable = VK_FALSE;
			multisampling.alphaToOneEnable = VK_FALSE;
		}
		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		{
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencil.depthBoundsTestEnable = VK_FALSE;
			depthStencil.stencilTestEnable = VK_FALSE;
		}
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		{
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_FALSE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		}
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		{
			colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlending.logicOpEnable = VK_FALSE;
			colorBlending.logicOp = VK_LOGIC_OP_COPY;
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &colorBlendAttachment;
			colorBlending.blendConstants[0] = 0.0f;
			colorBlending.blendConstants[1] = 0.0f;
			colorBlending.blendConstants[2] = 0.0f;
			colorBlending.blendConstants[3] = 0.0f;
		}
		/*static const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		{
			dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicState.dynamicStateCount = 2;
			dynamicState.pDynamicStates = dynamicStates;
		}*/

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		{
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = shaderStages;
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = &depthStencil;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.pDynamicState = nullptr;
			pipelineInfo.layout = m_pipelineLayout;
			pipelineInfo.renderPass = m_swapChain->renderPass();
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineInfo.basePipelineIndex = -1;
		}
		if (vkCreateGraphicsPipelines(m_graphicsDevice->logicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
			m_graphicsPipeline = VK_NULL_HANDLE;
			log("[Error] Renderer - Failed to create graphics pipeline.");
			return false;
		}
		return true;
	}

	bool Renderer::createDescriptorPool() {
		// Descriptor pool:
		{
			VkDescriptorPoolSize sizes[2];
			{
				VkDescriptorPoolSize& size = sizes[0];
				size = {};
				size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				size.descriptorCount = m_object->numLayoutBindings();
			}
			{
				VkDescriptorPoolSize& size = sizes[1];
				size = {};
				size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				size.descriptorCount = m_object->numLayoutBindings();
			}
			VkDescriptorPoolCreateInfo info = {};
			{
				info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				info.poolSizeCount = sizeof(sizes) / sizeof(VkDescriptorPoolSize);
				info.pPoolSizes = sizes;
				info.maxSets = m_swapChain->frameBufferCount();
			}
			if (vkCreateDescriptorPool(m_graphicsDevice->logicalDevice(), &info, nullptr, &m_descriptorPool) != VK_SUCCESS) {
				m_descriptorPool = VK_NULL_HANDLE;
				log("[Error] Renderer - Failed to create descriptor pool.");
				return false;
			}
		}
		// Descriptor sets:
		{
			VkDescriptorSetAllocateInfo info = {};
			{
				info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				info.descriptorPool = m_descriptorPool;
				info.descriptorSetCount = 1;
				info.pSetLayouts = &m_descriptorSetLayout;
			}
			if (vkAllocateDescriptorSets(m_graphicsDevice->logicalDevice(), &info, &m_descriptorSet) != VK_SUCCESS) {
				m_descriptorSet = VK_NULL_HANDLE;
				log("[Error] Renderer - Failed to allocate descriptor sets.");
				return false;
			}

			std::vector<VkWriteDescriptorSet> writes(m_object->numLayoutBindings());
			for (uint32_t descId = 0; descId < m_object->numLayoutBindings(); descId++) {
				VkWriteDescriptorSet write = m_object->descriptorBinding(descId);
				write.dstSet = m_descriptorSet;
				writes[descId] = write;
			}
			vkUpdateDescriptorSets(m_graphicsDevice->logicalDevice(), m_object->numLayoutBindings(), writes.data(), 0, nullptr);
		}
		return true;
	}

	bool Renderer::createCommandBuffers() {
		m_commandBuffers.resize(m_swapChain->frameBufferCount());
		{
			VkCommandBufferAllocateInfo info = {};
			{
				info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				info.commandPool = m_graphicsDevice->commandPool();
				info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				info.commandBufferCount = (uint32_t)m_commandBuffers.size();
			}
			if (vkAllocateCommandBuffers(m_graphicsDevice->logicalDevice(), &info, m_commandBuffers.data()) != VK_SUCCESS) {
				m_commandBuffers.clear();
				log("[Error] Renderer - failed to allocate command buffers!");
				return false;
			}
		}
		for (size_t i = 0; i < m_commandBuffers.size(); i++) {
			VkCommandBuffer commandBuffer = m_commandBuffers[i];
			{
				VkCommandBufferBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				info.flags = 0;
				info.pInheritanceInfo = nullptr;
				if (vkBeginCommandBuffer(commandBuffer, &info) != VK_SUCCESS) {
					m_commandBuffers.clear();
					log("[Error] Renderer - Failed to begin recording command buffer.");
					return false;
				}
			}
			{
				VkRenderPassBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				info.renderPass = m_swapChain->renderPass();
				info.framebuffer = m_swapChain->frameBuffer(i);
				info.renderArea.offset = { 0, 0 };
				info.renderArea.extent = m_swapChain->size();
				VkClearValue clearValues[2];
				{
					clearValues[0] = {};
					clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
				}
				{
					clearValues[1] = {};
					clearValues[1].depthStencil = { 1.0f, 0 };
				}
				info.clearValueCount = (sizeof(clearValues) / sizeof(VkClearValue));
				info.pClearValues = clearValues;
				vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
			}
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
			{
				VkBuffer vertexBuffers[] = { m_object->vertexBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			}
			vkCmdBindIndexBuffer(commandBuffer, m_object->indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, m_object->numIndices(), 1, 0, 0, 0);
			vkCmdEndRenderPass(commandBuffer);
			if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
				m_commandBuffers.clear();
				log("[Error] Renderer - Failed to end recording command buffer.");
				return false;
			}
		}
		return true;
	}

	void Renderer::clearSwapChainDependedObjects() {
		vkDeviceWaitIdle(m_graphicsDevice->logicalDevice());

		if (m_graphicsPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(m_graphicsDevice->logicalDevice(), m_graphicsPipeline, nullptr);
			m_graphicsPipeline = VK_NULL_HANDLE;
		}

		if (m_descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(m_graphicsDevice->logicalDevice(), m_descriptorPool, nullptr);
			m_descriptorPool = VK_NULL_HANDLE;
		}

		m_initialized = false;
	}

	void Renderer::recreateSwapChainDependedObjects() {
		if (m_pipelineLayout == VK_NULL_HANDLE) return;
		clearSwapChainDependedObjects();
		if (createRenderPipeline())
			if (createDescriptorPool())
				if (createCommandBuffers())
					m_initialized = true;
	}
}
