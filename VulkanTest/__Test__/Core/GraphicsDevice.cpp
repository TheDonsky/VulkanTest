#include "GraphicsDevice.h"
#include "SwapChain.h"
#include <sstream>
#include <unordered_set>

namespace {
#ifndef NDEBUG
#define ENABLE_VALIDATION_LAYERS
	std::vector<const char*> getValidationLayers(void(*logFn)(const char*)) {
		static const std::unordered_set<std::string> NEEDED_VALIDATION_LAYERS = {
			"VK_LAYER_KHRONOS_validation"
		};

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		std::vector<const char*> layers;
		for (size_t i = 0; i < layerCount; i++) {
			std::unordered_set<std::string>::const_iterator it = NEEDED_VALIDATION_LAYERS.find(availableLayers[i].layerName);
			if (it != NEEDED_VALIDATION_LAYERS.end())
				layers.push_back(it->c_str());
		}
		return layers;
	}

	inline static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, PFN_vkDebugUtilsMessengerCallbackEXT callback, void* userData) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = callback;
		createInfo.pUserData = userData;
	}

#endif

	static const uint64_t MIN_SEVERITY = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

	static const char* REQUIRED_DEVICE_EXTENSIONS[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#define REQUIRED_DEVICE_EXTENSION_COUNT (sizeof(REQUIRED_DEVICE_EXTENSIONS) / sizeof(const char*))
}

namespace Test {
	GraphicsDevice::GraphicsDevice(const std::shared_ptr<Window>& wnd, void(*logFn)(const char*))
		: m_window(wnd)
		, m_instance(VK_NULL_HANDLE)
		, m_debugMessenger(VK_NULL_HANDLE)
		, m_surface(VK_NULL_HANDLE)
		, m_physDevice(VK_NULL_HANDLE)
		, m_device(VK_NULL_HANDLE)
		, m_graphicsQueue(VK_NULL_HANDLE)
		, m_presentQueue(VK_NULL_HANDLE)
		, m_commandPool(VK_NULL_HANDLE)
		, m_complete(false)
		, m_logFn(logFn) {
		if (createVulkanInstance())
			if (createDebugMessenger())
				if (createSurface())
					if (selectPhysicalDevice())
						if (createLogicalDevice())
							if (createCommandPool())
								m_complete = true;
	}

	GraphicsDevice::~GraphicsDevice() {
		if (m_device != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(m_device);

			if (m_commandPool != VK_NULL_HANDLE)
				vkDestroyCommandPool(m_device, m_commandPool, nullptr);

			vkDestroyDevice(m_device, nullptr);
		}

		if (m_instance != nullptr) {
			if (m_surface != VK_NULL_HANDLE)
				vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

			if (m_debugMessenger != VK_NULL_HANDLE) {
				PFN_vkDestroyDebugUtilsMessengerEXT destroyFn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
				if (destroyFn != nullptr)
					destroyFn(m_instance, m_debugMessenger, nullptr);
				else log("[Error] GraphicsDevice - Could not find vkDestroyDebugUtilsMessengerEXT function.");
			}

			if (m_instance != VK_NULL_HANDLE)
				vkDestroyInstance(m_instance, nullptr);
		}
	}

	bool GraphicsDevice::initialized() {
		return m_complete;
	}

	VkPhysicalDevice GraphicsDevice::physicalDevice()const {
		return m_physDevice;
	}

	VkDevice GraphicsDevice::logicalDevice()const {
		return m_device;
	}

	VkQueue GraphicsDevice::graphicsQueue()const {
		return m_graphicsQueue;
	}

	VkQueue GraphicsDevice::presentQueue()const {
		return m_presentQueue;
	}

	VkCommandPool GraphicsDevice::commandPool()const {
		return m_commandPool;
	}

	VkSurfaceKHR GraphicsDevice::surface()const {
		return m_surface;
	}

	Window& GraphicsDevice::window()const {
		return *m_window;
	}

	const GraphicsDevice::QueueFamilies& GraphicsDevice::queueFamilies()const {
		return m_queueFamilies;
	}


	void GraphicsDevice::log(const char* message)const { 
		if (m_logFn != nullptr) 
			m_logFn(message); 
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL GraphicsDevice::debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		if (messageSeverity < MIN_SEVERITY) return VK_FALSE;

		GraphicsDevice* device = (GraphicsDevice*)pUserData;

		const std::string severityAsString =
			messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ? "Diagnostic" :
			messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ? "Informational" :
			messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "Warning" :
			messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "Error" :
			"UNKNWOWN SEVERITY";

		const std::string messageTypeAsTring =
			messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT ? "General" :
			messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Violation" :
			messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ? "Performance" :
			"UNKNOWN TYPE";

		std::stringstream stream;
		stream << "{GraphicsDevice - " << device
			<< "} [" << severityAsString << "; " << (int)messageSeverity << "] <"
			<< messageTypeAsTring << "; " << (int)messageType << "> \nMessage: " << pCallbackData->pMessage << std::endl;

		for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
			const VkDebugUtilsObjectNameInfoEXT& objectInfo = pCallbackData->pObjects[i];
			stream << "    Object " << i << ": {"
				<< "sType:" << objectInfo.sType
				<< "; objectType: " << objectInfo.objectType
				<< "; objectName:" << (objectInfo.pObjectName == nullptr ? "<NULL>" : objectInfo.pObjectName)
				<< "; objectHandle:" << objectInfo.objectHandle
				<< "}" << std::endl;
		}

		device->log(stream.str().c_str());

		return VK_FALSE;
	}

	bool GraphicsDevice::createVulkanInstance() {
		if (m_instance != VK_NULL_HANDLE) return true;

		VkApplicationInfo appInfo = {};
		{
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pNext = nullptr;
			appInfo.pApplicationName = "Vulkan Test";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;
		}

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
#ifdef ENABLE_VALIDATION_LAYERS
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		populateDebugMessengerCreateInfo(debugCreateInfo, debugCallback, this);
		createInfo.pNext = &debugCreateInfo;
#else 
		createInfo.pNext = nullptr;
#endif
		{
			uint32_t wndExtensionCount = 0;
			const char** wndExtensions = nullptr;
			Test::Window::getRequiredInstanceExtensions(wndExtensionCount, wndExtensions);
			m_extensions = std::vector<const char*>(wndExtensions, wndExtensions + wndExtensionCount);
#ifdef ENABLE_VALIDATION_LAYERS
			m_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
			createInfo.enabledExtensionCount = static_cast<uint32_t>(m_extensions.size());
			createInfo.ppEnabledExtensionNames = m_extensions.data();
		}
		{
#ifdef ENABLE_VALIDATION_LAYERS
			m_validationLayers = getValidationLayers(m_logFn);
			createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
			createInfo.ppEnabledLayerNames = m_validationLayers.data();
#else
			createInfo.enabledLayerCount = 0;
			createInfo.ppEnabledLayerNames = nullptr;
#endif	
		}
		
		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
			m_instance = VK_NULL_HANDLE;
			log("[Error] GraphicsDevice - Failed to create VkInstance.");
			return false;
		}
		return true;
	}

	bool GraphicsDevice::createDebugMessenger() {
		if (m_instance == VK_NULL_HANDLE) return false;
		else if (m_debugMessenger != VK_NULL_HANDLE) return true;
#ifdef ENABLE_VALIDATION_LAYERS
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo, debugCallback, this);

		static PFN_vkCreateDebugUtilsMessengerEXT createFn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
		if (createFn != nullptr) {
			if (createFn(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
				m_debugMessenger = VK_NULL_HANDLE;
				log("[Error] GraphicsDevice - Failed to create debug messenger.");
				return false;
			}
			else return true;
		}
		else {
			log("[Error] GraphicsDevice - Failed to find vkCreateDebugUtilsMessengerEXT function.");
			return false;
		}
#else 
		return true;
#endif
	}

	bool GraphicsDevice::createSurface() {
		m_surface = m_window->createSurface(m_instance);
		if (m_surface == VK_NULL_HANDLE) {
			log("[Error] GraphicsDevice - Failed to instantiate surface.");
			return false;
		}
		else return true;
	}

	bool GraphicsDevice::selectPhysicalDevice() {
		if (m_instance == nullptr) return false;

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
			log("[Error] GraphicsDevice - Device count is 0.");
			return false;
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

		size_t deviceId = deviceCount;
		for (size_t i = 0; i < deviceCount; i++)
			if (deviceId >= deviceCount && physicalDeviceSuitable(devices[i])) {
				// TODO: Maybe... Select the fastest device somehow...
				deviceId = i;
			}

		if (deviceId >= deviceCount) {
			log("[Error] GraphicsDevice - Could not pick a phisical device.");
			return false;
		}

		m_physDevice = devices[deviceId];
		m_queueFamilies = getQueueFamilies(m_physDevice);

		return true;
	}

	bool GraphicsDevice::physicalDeviceSuitable(VkPhysicalDevice device) {
		// Check for device properties:
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(device, &properties);
			if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return false;
		}

		// Check for device features:
		{
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(device, &features);
			if (!features.geometryShader) return false;
		}

		// Check extension support:
		{
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

			std::vector<VkExtensionProperties> extensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

			std::unordered_set<std::string> missingExtensions(
				REQUIRED_DEVICE_EXTENSIONS, REQUIRED_DEVICE_EXTENSIONS + REQUIRED_DEVICE_EXTENSION_COUNT);

			for (uint32_t i = 0; i < extensionCount; i++)
				missingExtensions.erase(extensions[i].extensionName);

			if (!missingExtensions.empty()) return false;
		}

		// Swap chain capabbilities:
		{
			SwapChain::SwapChainSupportInfo swapChainInfo = SwapChain::getSwapChainSupportInfo(device, m_surface);
			if (swapChainInfo.pixelFormats.empty() || swapChainInfo.presentModes.empty()) return false;
		}

		// Check for required queue families:
		{
			QueueFamilies queueFamilies = getQueueFamilies(device);
			if (!(queueFamilies.graphics.has_value() && queueFamilies.present.has_value())) return false;
		}

		return true;
	}

	GraphicsDevice::QueueFamilies GraphicsDevice::getQueueFamilies(VkPhysicalDevice device) {
		QueueFamilies families;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			const VkQueueFamilyProperties& properties = queueFamilies[i];
			if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
				families.graphics = i;

			{
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
				if (presentSupport)
					families.present = i;
			}
		}

		return families;
	}

	bool GraphicsDevice::createLogicalDevice() {
		if (m_physDevice == VK_NULL_HANDLE) return false;
		else if (m_device != VK_NULL_HANDLE) return true;

		VkDeviceQueueCreateInfo queueCreateInfos[2];
		const float queuePriority = 1.0f;
		uint32_t queueCreateInfoCount = 1;
		{
			VkDeviceQueueCreateInfo& info = queueCreateInfos[0];
			info = {};
			info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			info.queueFamilyIndex = m_queueFamilies.graphics.value();
			info.queueCount = 1;
			info.pQueuePriorities = &queuePriority;
		}
		if (m_queueFamilies.present != m_queueFamilies.graphics) {
			VkDeviceQueueCreateInfo& info = queueCreateInfos[queueCreateInfoCount];
			info = {};
			info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			info.queueFamilyIndex = m_queueFamilies.present.value();
			info.queueCount = 1;
			info.pQueuePriorities = &queuePriority;
			queueCreateInfoCount++;
		}

		VkPhysicalDeviceFeatures deviceFeatures = { VK_FALSE };

		VkDeviceCreateInfo createInfo = {};
		{
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pQueueCreateInfos = queueCreateInfos;
			createInfo.queueCreateInfoCount = queueCreateInfoCount;
			createInfo.pEnabledFeatures = &deviceFeatures;

			createInfo.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSION_COUNT);
			createInfo.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS;

#ifdef ENABLE_VALIDATION_LAYERS
			createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
			createInfo.ppEnabledLayerNames = m_validationLayers.data();
#else
			createInfo.enabledLayerCount = 0;
			createInfo.ppEnabledLayerNames = nullptr;
#endif	
		}

		if (vkCreateDevice(m_physDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
			m_device = VK_NULL_HANDLE;
			log("[Error] GraphicsDevice - Failed to create logical device.");
		}
		
		vkGetDeviceQueue(m_device, m_queueFamilies.graphics.value(), 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_device, m_queueFamilies.present.value(), 0, &m_presentQueue);
		return true;
	}

	bool GraphicsDevice::createCommandPool() {
		VkCommandPoolCreateInfo info = {};
		{
			info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			info.queueFamilyIndex = m_queueFamilies.graphics.value();
			info.flags = 0;
		}
		if (vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool) != VK_SUCCESS) {
			m_commandPool = VK_NULL_HANDLE;
			log("[Error] GraphicsDevice - Failed to create command pool.");
			return false;
		}
		return true;
	}
}

