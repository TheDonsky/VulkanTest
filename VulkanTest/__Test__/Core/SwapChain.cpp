#include "SwapChain.h"
#include <algorithm>

namespace {
	inline static VkSurfaceFormatKHR pickSurfaceFormat(const Test::SwapChain::SwapChainSupportInfo& info) {
		for (size_t i = 0; i < info.pixelFormats.size(); i++) {
			const VkSurfaceFormatKHR& format = info.pixelFormats[i];
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		return info.pixelFormats[0];
	}

	inline static VkPresentModeKHR pickPresentationMode(const Test::SwapChain::SwapChainSupportInfo& info) {
		for (size_t i = 0; i < info.presentModes.size(); i++)
			if (info.presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				return VK_PRESENT_MODE_MAILBOX_KHR;
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	inline static VkExtent2D pickResolution(const Test::SwapChain::SwapChainSupportInfo& info, const Test::Window& window) {
		return (info.capabilities.currentExtent.width < UINT32_MAX)
			? info.capabilities.currentExtent
			: VkExtent2D{ std::max(info.capabilities.minImageExtent.width, std::min(info.capabilities.maxImageExtent.width, (uint32_t)window.width()))
			, std::max(info.capabilities.minImageExtent.height, std::min(info.capabilities.maxImageExtent.height, (uint32_t)window.height())) };
	}
}


namespace Test {
	SwapChain::SwapChain(const std::shared_ptr<GraphicsDevice>& device, void(*logFn)(const char*))
		: m_device(device)
		, m_swapChain(VK_NULL_HANDLE), m_renderPass(VK_NULL_HANDLE)
		, m_pixelFormat{}, m_size{}
		, m_imageAvailable(VK_NULL_HANDLE), m_renderFinished(VK_NULL_HANDLE)
		, m_initialized(false), m_logFn(logFn) {
		if (m_device->initialized()) {
			VkSemaphoreCreateInfo semInfo = {};
			semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			if (vkCreateSemaphore(m_device->logicalDevice(), &semInfo, nullptr, &m_imageAvailable) != VK_SUCCESS)
				log("[Error] SwapChain - Could not create image available semaphore");
			else if (vkCreateSemaphore(m_device->logicalDevice(), &semInfo, nullptr, &m_renderFinished) != VK_SUCCESS)
				log("[Error] SwapChain - Could not create render finished semaphore");
			else recreateSwapChain();
		}
	}

	SwapChain::~SwapChain() {
		clearSwapChain();

		if (m_renderFinished != VK_NULL_HANDLE)
			vkDestroySemaphore(m_device->logicalDevice(), m_renderFinished, nullptr);

		if (m_imageAvailable != VK_NULL_HANDLE)
			vkDestroySemaphore(m_device->logicalDevice(), m_imageAvailable, nullptr);
	}

	SwapChain::RecreationListenerId SwapChain::addRecreationListener(const RecreationListener& listener) {
		std::unique_lock<std::mutex> lock(m_recreationLock);
		while (m_recreationListeners.find(m_recreationListenerIdCounter) != m_recreationListeners.end())
			m_recreationListenerIdCounter++;
		RecreationListenerId id = m_recreationListenerIdCounter;
		m_recreationListeners[id] = listener;
		m_recreationListenerIdCounter++;
		if (m_initialized)
			listener();
		return id;
	}

	void SwapChain::removeRecreationListener(RecreationListenerId listenerId) {
		std::unique_lock<std::mutex> lock(m_recreationLock);
		RecreationListeners::iterator it = m_recreationListeners.find(listenerId);
		if (it != m_recreationListeners.end())
			m_recreationListeners.erase(it);
	}

	bool SwapChain::initialized()const {
		std::unique_lock<std::mutex> lock(m_recreationLock);
		return m_initialized;
	}

	const VkExtent2D& SwapChain::size()const {
		return m_size;
	}

	const VkSurfaceFormatKHR& SwapChain::format()const {
		return m_pixelFormat;
	}

	VkRenderPass SwapChain::renderPass()const {
		return m_renderPass;
	}

	uint32_t SwapChain::frameBufferCount()const {
		return static_cast<uint32_t>(m_frameBuffers.size());
	}

	VkFramebuffer SwapChain::frameBuffer(size_t index)const {
		return m_frameBuffers[index];
	}

	Image& SwapChain::depthBuffer() {
		return *m_depthBuffer;
	}

	bool SwapChain::aquireNextImage(size_t& index, VkSemaphore*& semaphoreToWait, VkSemaphore*& renderSemaphore) {
		vkQueueWaitIdle(m_device->presentQueue()); // Suboptimal, I know..
		uint32_t id;
		VkResult result = vkAcquireNextImageKHR(m_device->logicalDevice(), m_swapChain, UINT64_MAX, m_imageAvailable, VK_NULL_HANDLE, &id);
		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
				recreateSwapChain();
			return false;
		}
		index = id;
		semaphoreToWait = &m_imageAvailable;
		renderSemaphore = &m_renderFinished;
		return true;
	}

	void SwapChain::present(size_t index) {
		VkPresentInfoKHR info = {};
		uint32_t id = static_cast<uint32_t>(index);
		{
			info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			info.waitSemaphoreCount = 1;
			info.pWaitSemaphores = &m_renderFinished;
			info.swapchainCount = 1;
			info.pSwapchains = &m_swapChain;
			info.pImageIndices = &id;
			info.pResults = nullptr;
		}
		VkResult result = vkQueuePresentKHR(m_device->presentQueue(), &info);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			recreateSwapChain();
	}

	
	SwapChain::SwapChainSupportInfo SwapChain::getSwapChainSupportInfo(VkPhysicalDevice device, VkSurfaceKHR surface) {
		SwapChainSupportInfo info = {};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &info.capabilities);
		{
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
			info.pixelFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, info.pixelFormats.data());
		}
		{
			uint32_t modeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
			info.presentModes.resize(modeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, info.presentModes.data());
		}
		return info;
	}


	void SwapChain::log(const char* message)const {
		if (m_logFn != nullptr)
			m_logFn(message);
	}

	bool SwapChain::createSwapChain() {
		if (m_swapChain != VK_NULL_HANDLE) return true;
		SwapChainSupportInfo info = getSwapChainSupportInfo(m_device->physicalDevice(), m_device->surface());
		VkSwapchainCreateInfoKHR createInfo = {};
		uint32_t queueFamilyIndices[2] = { m_device->queueFamilies().graphics.value(), m_device->queueFamilies().present.value() };
		{
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = m_device->surface();
			createInfo.minImageCount = std::min(info.capabilities.minImageCount + 1, (info.capabilities.maxImageCount <= 0) ? UINT32_MAX : info.capabilities.maxImageCount);
			m_pixelFormat = pickSurfaceFormat(info);
			createInfo.imageFormat = m_pixelFormat.format;
			createInfo.imageColorSpace = m_pixelFormat.colorSpace;
			createInfo.imageExtent = m_size = pickResolution(info, m_device->window());
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndices;
			}
			else {
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0; // Optional
				createInfo.pQueueFamilyIndices = nullptr; // Optional
			}
			createInfo.preTransform = info.capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode = pickPresentationMode(info);
			createInfo.clipped = VK_TRUE;
			createInfo.oldSwapchain = VK_NULL_HANDLE;
		}

		if (vkCreateSwapchainKHR(m_device->logicalDevice(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
			m_swapChain = VK_NULL_HANDLE;
			log("[Error] SwapChain - Failed to create swap chain.");
			return false;
		}

		return true;
	}

	void SwapChain::fetchImages() {
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(m_device->logicalDevice(), m_swapChain, &imageCount, nullptr);
		m_images.resize(imageCount);
		vkGetSwapchainImagesKHR(m_device->logicalDevice(), m_swapChain, &imageCount, m_images.data());
	}

	void SwapChain::clearImageViews() {
		for (size_t i = 0; i < m_imageViews.size(); i++)
			vkDestroyImageView(m_device->logicalDevice(), m_imageViews[i], nullptr);
		m_imageViews.clear();
	}

	bool SwapChain::createImageViews() {
		clearImageViews();
		for (size_t i = 0; i < m_images.size(); i++) {
			VkImageViewCreateInfo createInfo = {};
			{
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = m_images[i];
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = m_pixelFormat.format;
			}
			{
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			}
			{
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;
			}
			VkImageView view;
			if (vkCreateImageView(m_device->logicalDevice(), &createInfo, nullptr, &view) != VK_SUCCESS) {
				log("[Error] SwapChain - Failed to create image view.");
				clearImageViews();
				return false;
			}
			else m_imageViews.push_back(view);
		}
		return true;
	}

	bool SwapChain::createDepthBuffer() {
		static const VkFormat possibleFormats[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		static const size_t numPossibleFormats = (sizeof(possibleFormats) / sizeof(VkFormat));
		VkFormat format;
		bool formatFound = false;
		for (size_t i = 0; i < numPossibleFormats; i++) {
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(m_device->physicalDevice(), possibleFormats[i], &properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				format = possibleFormats[i];
				formatFound = true;
			}
		}
		if (!formatFound) {
			log("[Error] SwapChain - Failed to determine depth image format.");
			return false;
		}
		m_depthBuffer = std::make_unique<Image>(m_device, m_size, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, m_logFn);
		if (m_depthBuffer == nullptr || (!m_depthBuffer->initialized()))
			m_depthBuffer.reset();
		return (m_depthBuffer != nullptr);
	}

	bool SwapChain::createRenderPass() {
		VkAttachmentDescription attachments[2];

		VkAttachmentDescription& colorAttachment = attachments[0];
		{
			colorAttachment = {};
			colorAttachment.format = m_pixelFormat.format;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
		VkAttachmentReference colorAttachmentRef = {};
		{
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		
		VkAttachmentDescription& depthAttachment = attachments[1];
		{
			depthAttachment = {};
			depthAttachment.format = m_depthBuffer->format();
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		VkAttachmentReference depthAttachmentRef = {};
		{
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription subpass = {};
		{
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;
		}
		VkSubpassDependency dependency = {};
		{
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		VkRenderPassCreateInfo info = {};
		{
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			info.attachmentCount = (sizeof(attachments) / sizeof(VkAttachmentDescription));
			info.pAttachments = attachments;
			info.subpassCount = 1;
			info.pSubpasses = &subpass;
			info.dependencyCount = 1;
			info.pDependencies = &dependency;
		}
		if (vkCreateRenderPass(m_device->logicalDevice(), &info, nullptr, &m_renderPass) != VK_SUCCESS) {
			m_renderPass = VK_NULL_HANDLE;
			log("[Error] SwapChain - Failed to create a render pass.");
			return false;
		}

		return true;
	}

	void SwapChain::clearFrameBuffers() {
		for (size_t i = 0; i < m_frameBuffers.size(); i++)
			vkDestroyFramebuffer(m_device->logicalDevice(), m_frameBuffers[i], nullptr);
		m_frameBuffers.clear();
	}

	bool SwapChain::createFrameBuffers() {
		clearFrameBuffers();
		for (size_t i = 0; i < m_imageViews.size(); i++) {
			VkImageView attachments[] = { m_imageViews[i], m_depthBuffer->view() };
			VkFramebufferCreateInfo info = {};
			{
				info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				info.renderPass = m_renderPass;
				info.attachmentCount = (sizeof(attachments) / sizeof(VkImageView));
				info.pAttachments = attachments;
				info.width = m_size.width;
				info.height = m_size.height;
				info.layers = 1;
			}
			VkFramebuffer buffer;
			if (vkCreateFramebuffer(m_device->logicalDevice(), &info, nullptr, &buffer) != VK_SUCCESS) {
				clearFrameBuffers();
				log("[Error] SwapChain - Failed to create frame buffers.");
				return false;
			}
			else m_frameBuffers.push_back(buffer);
		}
		return true;
	}

	void SwapChain::recreateSwapChain() {
		std::unique_lock<std::mutex> lock(m_recreationLock);

		clearSwapChain();
		m_initialized = false;

		if (createSwapChain()) {
			fetchImages();
			if (createImageViews())
				if (createDepthBuffer())
					if (createRenderPass())
						if (createFrameBuffers())
							m_initialized = true;
		}

		if (m_initialized)
			for (RecreationListeners::const_iterator it = m_recreationListeners.begin(); it != m_recreationListeners.end(); ++it)
				it->second();
	}

	void SwapChain::clearSwapChain() {
		vkDeviceWaitIdle(m_device->logicalDevice());

		clearFrameBuffers();

		if (m_renderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(m_device->logicalDevice(), m_renderPass, nullptr);
			m_renderPass = VK_NULL_HANDLE;
		}

		clearImageViews();

		if (m_swapChain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(m_device->logicalDevice(), m_swapChain, nullptr);
			m_swapChain = VK_NULL_HANDLE;
		}
	}
}
