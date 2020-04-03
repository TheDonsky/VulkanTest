#pragma once
#include "../Core/SwapChain.h"

namespace Test {
	/**
	 * Interface for object, responsible for directing Renderer's behaviour.
	 * Responsibilities of derived classes include things like giving information about the shaders,
	 * all the bindings, providing all the required buffers and fun stuff like that.
	 */
	class IRenderObject {
	public:
		/** 
		Since copy-constructors are disabled, we need to explicitly clarify to the compiler, 
		that we indeed will need to create an instance at some point and there's no harm in that. 
		*/
		inline IRenderObject() {}

		/** Virtual destructor... Because interfaces... */
		virtual inline ~IRenderObject() {}

		/**
		In case something went wrong during initialisation, this method should tell us about it.
		@return true, if the object is fully functional.
		*/
		virtual bool initialized() = 0;

		/**
		Vertex shader.
		@return relative or absolute path to the compiled vertex shader SPV file.
		*/
		virtual const char* vertexShader() = 0;

		/**
		Fargment shader.
		@return relative or absolute path to the compiled fragment shader SPV file.
		*/
		virtual const char* fragmentShader() = 0;

		/**
		Should provide vertex input description (keep in mind, that nobody will clear binding and attribute description memories and better keep em static).
		@return pre-filled vertex input descriptor.
		*/
		virtual VkPipelineVertexInputStateCreateInfo vertexInputInfo() = 0;

		/**
		Number of vertices within the vertex buffer.
		@return vertex input count.
		*/
		virtual uint32_t numVertices() = 0;

		/**
		Reference to a buffer, containing vertex data.
		@return vertex buffer for vertex shader input.
		*/
		virtual VkBuffer vertexBuffer() = 0;
		
		/**
		Number of wntries within the index buffer.
		@return index input count.
		*/
		virtual uint32_t numIndices() = 0;

		/**
		Reference to a buffer, containing index data.
		@return index buffer for vertex shader input.
		*/
		virtual VkBuffer indexBuffer() = 0;

		/**
		Should return the amount of layout bindings required for bothe vertex and fragment shaders.
		@return number of bindings.
		*/
		virtual uint32_t numLayoutBindings() = 0;

		/**
		Layout binding by index for building input layout.
		@param index Index of the binding (valid values are from 0(inclusive) to whatever numLayoutBindings returns(exclusive)).
		@return binding descriptor.
		*/
		virtual VkDescriptorSetLayoutBinding layoutBinding(uint32_t index) = 0;

		/**
		Layout binding by index for writing the inputs.
		@param index Index of the binding (valid values are from 0(inclusive) to whatever numLayoutBindings returns(exclusive)).
		@return binding descriptor.
		*/
		virtual VkWriteDescriptorSet descriptorBinding(uint32_t index) = 0;

		/**
		Called before rendering to let us update any buffers that may be outdated and/or desparately need new content.
		*/
		virtual void updateResources() = 0;


	private:
		IRenderObject(const IRenderObject&) = delete;
		IRenderObject& operator=(const IRenderObject&) = delete;
	};
}
