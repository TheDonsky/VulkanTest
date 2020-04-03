#include "Buffers.h"

namespace {
	inline static VkDeviceMemory allocateMemory(
		Test::GraphicsDevice& device,
		VkDeviceSize size, uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryType,
		void(*logFn)(const char*)) {
		
		VkMemoryAllocateInfo allocInfo = {};
		{
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = size;

			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(device.physicalDevice(), &memProperties);

			allocInfo.memoryTypeIndex = memProperties.memoryTypeCount;
			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
				if ((memoryTypeBits & (1 << i)) != 0
					&& ((memProperties.memoryTypes[i].propertyFlags & memoryType) == memoryType)) {
					allocInfo.memoryTypeIndex = i;
					break;
				}

			if (allocInfo.memoryTypeIndex == memProperties.memoryTypeCount) {
				if (logFn != nullptr) logFn("[Error] Memory type id not found.");
				return VK_NULL_HANDLE;
			}
		}

		VkDeviceMemory memory;
		if (vkAllocateMemory(device.logicalDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
			if (logFn != nullptr) logFn("[Error] Could not allocate memory.");
			return VK_NULL_HANDLE;
		}
		return memory;
	}

	inline static bool createBuffer(
		Test::GraphicsDevice& device, 
		VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryType,
		VkBuffer& buffer, VkDeviceMemory& memory, 
		void(*logFn)(const char*)) {

		VkBufferCreateInfo bufferInfo = {};
		{
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		if (vkCreateBuffer(device.logicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			buffer = VK_NULL_HANDLE;
			if (logFn != nullptr) logFn("[Error] createBuffer - Failed to instantiate vertex buffer.");
			return false;
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device.logicalDevice(), buffer, &memRequirements);

		memory = allocateMemory(device, memRequirements.size, memRequirements.memoryTypeBits, memoryType, logFn);
		if (memory == VK_NULL_HANDLE)
			return false;

		vkBindBufferMemory(device.logicalDevice(), buffer, memory, 0);
		return true;
	}

	inline static VkCommandBuffer createCopyOperation(Test::GraphicsDevice& device, VkDeviceSize size, VkBuffer src, VkBuffer dst) {
		VkCommandBufferAllocateInfo allocInfo = {};
		{
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = device.commandPool();
			allocInfo.commandBufferCount = 1;
		}
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device.logicalDevice(), &allocInfo, &commandBuffer);
		{
			VkCommandBufferBeginInfo begin = {};
			begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin.flags = 0;
			vkBeginCommandBuffer(commandBuffer, &begin);
		}
		{
			VkBufferCopy copy = {};
			copy.srcOffset = 0;
			copy.dstOffset = 0;
			copy.size = size;
			vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copy);
		}
		vkEndCommandBuffer(commandBuffer);
		return commandBuffer;
	}
}


namespace Test {
	/** 
	 * #############################################################################
	 * ######################### BASE STAGING BUFFER ############################### 
	 * #############################################################################
	 */
	BaseStagingBuffer::BaseStagingBuffer(const std::shared_ptr<GraphicsDevice>& device, VkBufferUsageFlags usage, uint32_t size, const void* data, void(*logFn)(const char*))
		: m_device(device), m_size(size)
		, m_stagingBuffer(VK_NULL_HANDLE), m_stagingBufferMemory(VK_NULL_HANDLE) {
		
		if (createBuffer(*m_device, size,
			usage, (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
			m_stagingBuffer, m_stagingBufferMemory, logFn))
			if (data != nullptr)
				setStagingBufferData(data);
	}

	BaseStagingBuffer::~BaseStagingBuffer() {
		if (m_stagingBuffer != VK_NULL_HANDLE)
			vkDestroyBuffer(m_device->logicalDevice(), m_stagingBuffer, nullptr);

		if (m_stagingBufferMemory != VK_NULL_HANDLE)
			vkFreeMemory(m_device->logicalDevice(), m_stagingBufferMemory, nullptr);
	}

	GraphicsDevice& BaseStagingBuffer::graphicsDevice() {
		return *m_device;
	}

	VkBuffer BaseStagingBuffer::stagingBuffer()const {
		return m_stagingBuffer;
	}

	uint32_t BaseStagingBuffer::numBytes()const {
		return m_size;
	}

	void* BaseStagingBuffer::mapStagingBuffer() {
		void* data;
		vkMapMemory(m_device->logicalDevice(), m_stagingBufferMemory, 0, m_size, 0, &data);
		return data;
	}

	void BaseStagingBuffer::setStagingBufferData(const void* data) {
		void* stagingData = mapStagingBuffer();
		memcpy(stagingData, data, m_size);
		unmapStagingBuffer();
	}

	void BaseStagingBuffer::unmapStagingBuffer() {
		vkUnmapMemory(m_device->logicalDevice(), m_stagingBufferMemory);
	}

	BaseBuffer::BaseBuffer(const std::shared_ptr<GraphicsDevice>& device, VkBufferUsageFlags usage, uint32_t size, const void* data, void(*logFn)(const char*))
		: BaseStagingBuffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, data, logFn)
		, m_buffer(VK_NULL_HANDLE), m_bufferMemory(VK_NULL_HANDLE)
		, m_commandBuffer(VK_NULL_HANDLE) {
		if (createBuffer(graphicsDevice(), size,
			(VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_buffer, m_bufferMemory, logFn)) {
			m_commandBuffer = createCopyOperation(graphicsDevice(), size, stagingBuffer(), m_buffer);
			if (data != nullptr)
				setData(data);
		}
	}





	/**
	 * #############################################################################
	 * ############################## BASE BUFFER ##################################
	 * #############################################################################
	 */
	BaseBuffer::~BaseBuffer() {
		vkDeviceWaitIdle(graphicsDevice().logicalDevice());

		if (m_commandBuffer != VK_NULL_HANDLE)
			vkFreeCommandBuffers(graphicsDevice().logicalDevice(), graphicsDevice().commandPool(), 1, &m_commandBuffer);

		if (m_buffer != VK_NULL_HANDLE)
			vkDestroyBuffer(graphicsDevice().logicalDevice(), m_buffer, nullptr);

		if (m_bufferMemory != VK_NULL_HANDLE)
			vkFreeMemory(graphicsDevice().logicalDevice(), m_bufferMemory, nullptr);
	}

	void* BaseBuffer::mapData() {
		return mapStagingBuffer();
	}

	void BaseBuffer::unmapData() {
		unmapStagingBuffer();
		VkSubmitInfo info = {};
		{
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.commandBufferCount = 1;
			info.pCommandBuffers = &m_commandBuffer;
		}
		vkQueueSubmit(graphicsDevice().graphicsQueue(), 1, &info, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsDevice().graphicsQueue());
	}

	void BaseBuffer::setData(const void* data) {
		void* mappedData = mapData();
		memcpy(mappedData, data, numBytes());
		unmapData();
	}

	VkBuffer BaseBuffer::buffer()const {
		return m_buffer;
	}

	uint32_t BaseBuffer::sizeInBytes()const {
		return numBytes();
	}





	/**
	 * #############################################################################
	 * ############################### Image #######################################
	 * #############################################################################
	 */
	Image::Image(const std::shared_ptr<GraphicsDevice>& device,
		const VkExtent2D& size, VkFormat imageFormat, VkImageTiling imageTiling,
		VkImageUsageFlags usage, VkImageAspectFlags viewAspectFlags,
		void(*logFn)(const char*))
		: m_device(device), m_size(size), m_format(imageFormat)
		, m_image(VK_NULL_HANDLE), m_memory(VK_NULL_HANDLE), m_view(VK_NULL_HANDLE)
		, m_logFn(logFn) {
		// Create Image:
		{
			VkImageCreateInfo info = {};
			{
				info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				info.imageType = VK_IMAGE_TYPE_2D;
				info.extent = { size.width, size.height, 1 };
				info.mipLevels = 1;
				info.arrayLayers = 1;
				info.format = m_format;
				info.tiling = imageTiling;
				info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				info.usage = usage;
				info.samples = VK_SAMPLE_COUNT_1_BIT;
				info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			}
			if (vkCreateImage(m_device->logicalDevice(), &info, nullptr, &m_image) != VK_SUCCESS) {
				m_image = VK_NULL_HANDLE;
				log("[Error] Image - Failed to create image.");
				return;
			}
		}
		// Create memory and bind to image:
		{
			VkMemoryRequirements requirements;
			vkGetImageMemoryRequirements(m_device->logicalDevice(), m_image, &requirements);
			m_memory = allocateMemory(*m_device, requirements.size, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_logFn);
			if (m_memory == VK_NULL_HANDLE)
				return;
			vkBindImageMemory(m_device->logicalDevice(), m_image, m_memory, 0);
		}
		// Create image view:
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = m_image;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = imageFormat;
			info.subresourceRange.aspectMask = viewAspectFlags;
			info.subresourceRange.baseMipLevel = 0;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.baseArrayLayer = 0;
			info.subresourceRange.layerCount = 1;

			if (vkCreateImageView(m_device->logicalDevice(), &info, nullptr, &m_view) != VK_SUCCESS) {
				m_view = VK_NULL_HANDLE;
				log("[Error] Image - Failed to create texture image view.");
				return;
			}
		}
	}

	Image::~Image() {
		vkDeviceWaitIdle(m_device->logicalDevice());

		if (m_view != VK_NULL_HANDLE)
			vkDestroyImageView(m_device->logicalDevice(), m_view, nullptr);

		if (m_image != VK_NULL_HANDLE)
			vkDestroyImage(m_device->logicalDevice(), m_image, nullptr);

		if (m_memory != VK_NULL_HANDLE)
			vkFreeMemory(m_device->logicalDevice(), m_memory, nullptr);
	}

	bool Image::initialized()const {
		return (m_image != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE && m_view != VK_NULL_HANDLE);
	}

	VkFormat Image::format()const {
		return m_format;
	}

	VkImageView Image::view()const {
		return m_view;
	}



	void Image::log(const char* message)const { 
		if (m_logFn != nullptr) 
			m_logFn(message); 
	}
}
