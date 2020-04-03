#pragma once
#include "../Objects/Buffers.h"
#include "RenderObject.h"

namespace Test {
	/**
	 * Wrapper of the entire render pipeline, responsible for rendering to and displaying images,
	 * hoever, still a slave to a RenderObject that ultimately dictates as of how the pipeline should behave once run.
	 */
	class Renderer {
	public:
		/**
		Instantiates a renderer.
		@param device Graphics device reference.
		@param swapChain Swap chain reference.
		@param object The only one who knows, how to configure the pipeline, what shaders to use, what inputs to provide and fun stuff like that.
		@param logFn Good old boring log function, you've probably got tired of reading about by this point.
		*/
		Renderer(
			const std::shared_ptr<GraphicsDevice>& device, const std::shared_ptr<SwapChain>& swapChain, 
			const std::shared_ptr<IRenderObject>& object, void(*logFn)(const char*) = nullptr);

		/** Destructor (obviously) */
		~Renderer();

		/**
		Tells, if everything went OK during instantiation.
		@return false, if something went wrong.
		*/
		bool initialized();

		/** 
		Renders a frame.
		*/
		void render();


	private:
		const std::shared_ptr<GraphicsDevice> m_graphicsDevice;
		const std::shared_ptr<SwapChain> m_swapChain;
		
		const std::shared_ptr<IRenderObject> m_object;

		VkShaderModule m_vertexShaderModule;
		VkShaderModule m_fragmentShaderModule;

		VkDescriptorSetLayout m_descriptorSetLayout;
		VkPipelineLayout m_pipelineLayout;
		VkPipeline m_graphicsPipeline;

		VkDescriptorPool m_descriptorPool;
		VkDescriptorSet m_descriptorSet;

		std::vector<VkCommandBuffer> m_commandBuffers;

		bool m_initialized;

		SwapChain::RecreationListenerId m_swapChainRecreationListenerId;

		void(*m_logFn)(const char*);


		void log(const char* message)const;

		bool createDescriptorSetLayout();

		bool createPipelineLayout();

		bool createRenderPipeline();

		bool createDescriptorPool();

		bool createCommandBuffers();

		void clearSwapChainDependedObjects();

		void recreateSwapChainDependedObjects();
	};
}
