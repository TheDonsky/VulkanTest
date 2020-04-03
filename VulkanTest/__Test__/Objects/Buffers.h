#pragma once
#include "../Core/GraphicsDevice.h"

/** 
Note for all buffers:
	I understand, that the GPU allocation count will be limited and it would be better, 
	if we shared memory allocation between buffers, but implementing that sort of a mechanism would be rather time consuming and unnecessary for the current project.
*/
namespace Test {
	/**
	 * A basic staging buffer, serving as a parent class for uniform buffers and things like that.
	 */
	class BaseStagingBuffer {
	public:
		/**
		Instantiates a staging buffer.
		@param device Graphics device handle.
		@param usage Desired usage of the buffer.
		@param size Size of the buffer in bytes.
		@param data Initail data to fill the buffer with (optional).
		@param logFn Logging function for error reporting (optional).
		*/
		BaseStagingBuffer(const std::shared_ptr<GraphicsDevice>& device, VkBufferUsageFlags usage, uint32_t size, const void* data = nullptr, void(*logFn)(const char*) = nullptr);

		/** Destructor */
		virtual ~BaseStagingBuffer();

		/** 
		Exposes underlying buffer reference. 
		@return Vulkan buffer.
		*/
		VkBuffer stagingBuffer()const;


	protected:
		// Size in bytes.
		uint32_t numBytes()const;

		// Maps data and returns readable-writable pointer to the GPU memory.
		void* mapStagingBuffer();

		// Sets the contant of the entire buffer.
		void setStagingBufferData(const void* data);

		// Unmaps staging buffer data.
		void unmapStagingBuffer();

		// Gives access to the graphics device.
		GraphicsDevice& graphicsDevice();


	private:
		const std::shared_ptr<GraphicsDevice> m_device;
		uint32_t m_size;
		VkBuffer m_stagingBuffer;
		VkDeviceMemory m_stagingBufferMemory;

		BaseStagingBuffer(const BaseStagingBuffer&) = delete;
		BaseStagingBuffer& operator=(const BaseStagingBuffer&) = delete;
	};


	/**
	 * A basic buffer, serving as a base class for vertex/index/storage buffers that require CPU inaccessible memory for performance.
	 */
	class BaseBuffer : private BaseStagingBuffer {
	public: 
		/**
		Instantiates a buffer.
		@param device Graphics device handle.
		@param usage Desired usage of the buffer.
		@param size Size of the buffer in bytes.
		@param data Initail data to fill the buffer with (optional).
		@param logFn Logging function for error reporting (optional).
		*/
		BaseBuffer(const std::shared_ptr<GraphicsDevice>& device, VkBufferUsageFlags usage, uint32_t size, const void* data = nullptr, void(*logFn)(const char*) = nullptr);

		/** Destructor */
		virtual ~BaseBuffer();

		/**
		Exposes underlying buffer reference.
		@return Vulkan buffer.
		*/
		VkBuffer buffer()const;


	protected:
		// Number of bytes within the buffer.
		uint32_t sizeInBytes()const;

		// Maps buffer data for write-only operations and returns mapped memory.
		void* mapData();

		// Sets content of the entire buffer memory.
		void setData(const void* data);

		// Unmaps buffer data and updates the memory.
		void unmapData();


	private:
		VkBuffer m_buffer;
		VkDeviceMemory m_bufferMemory;
		VkCommandBuffer m_commandBuffer;

		BaseBuffer(const BaseBuffer&) = delete;
		BaseBuffer& operator=(const BaseBuffer&) = delete;
	};


	/**
	 * Generic buffer with element type.
	 * @typeparam ElemType type of an individual element within the buffer (should be a good old plain data type with no internal allocations for the library not to go nuts).
	 * @typeparam usageType Buffer usage.
	 */
	template<typename ElemType, VkBufferUsageFlags usageType>
	class Buffer : public BaseBuffer {
	public:
		/**
		Instantiates a buffer.
		@param device Graphics device handle.
		@param count Element count.
		@param elems Initail elements to fill the buffer with (optional).
		@param logFn Logging function for error reporting (optional).
		*/
		inline Buffer(const std::shared_ptr<GraphicsDevice>& device, uint32_t count, const ElemType* elems = nullptr, void(*logFn)(const char*) = nullptr)
			: BaseBuffer(device, usageType, sizeof(ElemType)* count, elems, logFn) {}

		/**
		Instantiates a buffer from a single element.
		@param device Graphics device handle.
		@param value Value of the only element within the buffer.
		@param logFn Logging function for error reporting (optional).
		*/
		inline Buffer(const std::shared_ptr<GraphicsDevice>& device, const ElemType& value, void(*logFn)(const char*) = nullptr)
			: Buffer(device, 1, &value, logFn) {}

		/**
		Size of the buffer.
		@return amount of elements within the buffer.
		*/
		inline uint32_t size()const { return sizeInBytes() / sizeof(ElemType); }

		/**
		Maps buffer to CPU memory for write-only operations.
		@return mapped memory.
		*/
		inline ElemType* mapForWrite() { return (ElemType*)mapData(); }

		/**
		Unmaps buffer from CPU and updates GPU data.
		*/
		inline void unmap() { unmapData(); }

		/**
		Updates the content of the entire buffer.
		@param content Content to set (should point to an array which has no less elements than the buffer).
		*/
		inline void setContent(const ElemType* content) { setData(content); }
	};


	/**
	 * Generic staging buffer with element type.
	 * @typeparam ElemType type of an individual element within the buffer (should be a good old plain data type with no internal allocations for the library not to go nuts).
	 * @typeparam usageType Buffer usage.
	 */
	template<typename ElemType, VkBufferUsageFlags usageType>
	class StagingBuffer : public BaseStagingBuffer {
	public:
		/**
		Instantiates a buffer.
		@param device Graphics device handle.
		@param count Element count.
		@param elems Initail elements to fill the buffer with (optional).
		@param logFn Logging function for error reporting (optional).
		*/
		inline StagingBuffer(const std::shared_ptr<GraphicsDevice>& device, uint32_t count, const ElemType* elems = nullptr, void(*logFn)(const char*) = nullptr)
			: BaseStagingBuffer(device, usageType, sizeof(ElemType)* count, elems, logFn) {}

		/**
		Instantiates a buffer from a single element.
		@param device Graphics device handle.
		@param value Value of the only element within the buffer.
		@param logFn Logging function for error reporting (optional).
		*/
		inline StagingBuffer(const std::shared_ptr<GraphicsDevice>& device, const ElemType& value, void(*logFn)(const char*) = nullptr)
			: StagingBuffer(device, 1, &value, logFn) {}

		/**
		Size of the buffer.
		@return amount of elements within the buffer.
		*/
		inline uint32_t size()const { return numBytes() / sizeof(ElemType); }

		/**
		Maps buffer to CPU memory for read or write operations.
		@return mapped memory.
		*/
		inline ElemType* map() { return (ElemType*)mapStagingBuffer(); }

		/**
		Unmaps buffer from CPU and updates GPU data.
		*/
		inline void unmap() { unmapStagingBuffer(); }

		/**
		Updates the content of the entire buffer.
		@param content Content to set (should point to an array which has no less elements than the buffer).
		*/
		inline void setContent(const ElemType* content) { setStagingBufferData(content); }
	};


	/**
	 * Wrapper of a vertex buffer for an arbitrary vertex type.
	 * Can also be used as a storage buffer.
	 * @typeparam VertexType Type of the vertex element.
	 */
	template<typename VertexType>
	class VertexBuffer : public Buffer<VertexType, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> {
	public:
		/**
		Instantiates a vertex buffer.
		@param device Graphics device handle.
		@param count Element count.
		@param elems Initail elements to fill the buffer with (optional).
		@param logFn Logging function for error reporting (optional).
		*/
		inline VertexBuffer(const std::shared_ptr<GraphicsDevice>& device, uint32_t count, const VertexType* elems = nullptr, void(*logFn)(const char*) = nullptr)
			: Buffer<VertexType, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT>(device, count, elems, logFn) {}
	};

	/**
	 * Wrapper of an index buffer.
	 * Can also be used as a storage buffer.
	 */
	typedef Buffer<uint32_t, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT> IndexBuffer;

	/**
	 * Wrapper of an uniform buffer for an arbitrary value type.
	 * @typeparam BufferType Type of the buffer.
	 */
	template<typename BufferType>
	class ConstantBuffer : public StagingBuffer<BufferType, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT> {
	public:
		/**
		Instantiates an uniform buffer.
		@param device Graphics device handle.
		@param content Initail content to fill the buffer with (optional).
		@param logFn Logging function for error reporting (optional).
		*/
		inline ConstantBuffer(const std::shared_ptr<GraphicsDevice>& device, const BufferType* content = nullptr, void(*logFn)(const char*) = nullptr)
			: StagingBuffer<BufferType, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(device, 1, content, logFn) {}
	};

	
	/**
	 * Wrapper for managing a Vulkan image instance, alongside it's memory and view.
	 */
	class Image {
	public:
		/**
		Creates an image.
		@param device Graphics device handle.
		@param size Image extents.
		@param imageFormat Pixel format.
		@param imageTiling Image tiling options.
		@param usage Image usage.
		@param viewAspectFlags View aspect flags.
		@param logFn Logging function for error reporting (optional).
		*/
		Image(const std::shared_ptr<GraphicsDevice>& device, 
			const VkExtent2D &size, VkFormat imageFormat, VkImageTiling imageTiling,
			VkImageUsageFlags usage, VkImageAspectFlags viewAspectFlags,
			void(*logFn)(const char*) = nullptr);

		/** Destructor */
		virtual ~Image();

		/**
		Tells, if the image was successfully created or not.
		@return true, if image is fully functional.
		*/
		bool initialized()const;

		/**
		Image pixel format.
		@return surface format of the image.
		*/
		VkFormat format()const;

		/**
		Reference to the image view (Probably there was no need for the image view to be here, but it's all right, I guess...).
		@return complete view of the image.
		*/
		VkImageView view()const;


	private:
		const std::shared_ptr<GraphicsDevice>& m_device;
		VkExtent2D m_size;
		VkFormat m_format;
		VkImage m_image;
		VkDeviceMemory m_memory;
		VkImageView m_view;

		void(*m_logFn)(const char*);

		void log(const char* message)const;

		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;
	};
}

