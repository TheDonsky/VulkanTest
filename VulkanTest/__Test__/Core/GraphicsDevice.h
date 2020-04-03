#pragma once
#include "Window.h"
#include <vector>
#include <memory>
#include <optional>

namespace Test {
	/**
	 * Basic high level wrapper for initializing and being responsible for the lifecycle of common vulkan objects 
	 * like instance, pysical device and logical device, as well as the necessary queues that can be shared between different objects.
	 */
	class GraphicsDevice {
	public:
		/**
		 * Queue family indexes for a physical device.
		 */
		struct QueueFamilies {
			typedef std::optional<uint32_t> Id;

			// Graphics queue index.
			Id graphics;

			// Present queue index (can be the same as graphics).
			Id present;
		};


	public:
		/**
		Constructs a graphics device that can output to screen.
		Note: 
			Currently, the implementation makes sure, the physical device that is picked is discrete to prevent accidentally going with something like an igpu.
			Feel free to modify physicalDeviceSuitable() method if that is an issue.
		@param wnd Window we want to output to.
		@param logFn Log function for error reporing, as well as for the debug mode validation layers.
		*/
		GraphicsDevice(const std::shared_ptr<Window>& wnd, void(*logFn)(const char*) = nullptr);

		/** Destructor */
		~GraphicsDevice();

		/**
		Since we don't have initilization function and the constructor is responsible for most of it and we are not using exceptions, 
		most objects have initilized() method to make sure everything's ok.
		@return true, if GraphicsDevie is well formed.
		*/
		bool initialized();

		/**
		Physical device that was picked.
		@return handle to the physical device.
		*/
		VkPhysicalDevice physicalDevice()const;

		/**
		Vulkan device.
		@return handle to the logical device.
		*/
		VkDevice logicalDevice()const;

		/**
		Graphics queue for rendering tasks.
		@return handle of the queue.
		*/
		VkQueue graphicsQueue()const;

		/**
		Queue for presentation.
		@return handle of the queue.
		*/
		VkQueue presentQueue()const;
		
		/**
		Graphics command pool.
		@return handle to the command pool for rendering tasks.
		*/
		VkCommandPool commandPool()const;

		/**
		Vulkan surface for the target window.
		@return handle of the window surface.
		*/
		VkSurfaceKHR surface()const;

		/**
		Window, the graphics device is tied to.
		@return reference to the target window.
		*/
		Window& window()const;

		/**
		Queue family indices.
		@return indices for available vulkan queue families.
		*/
		const QueueFamilies& queueFamilies()const;





	private:
		const std::shared_ptr<Window> m_window;

		VkInstance m_instance;

		VkDebugUtilsMessengerEXT m_debugMessenger;

		VkSurfaceKHR m_surface;

		VkPhysicalDevice m_physDevice;

		QueueFamilies m_queueFamilies;

		VkDevice m_device;

		VkQueue m_graphicsQueue;

		VkQueue m_presentQueue;

		VkCommandPool m_commandPool;

		std::vector<const char*> m_extensions;

		std::vector<const char*> m_validationLayers;

		bool m_complete;
		
		void(*m_logFn)(const char*);



		void log(const char* message)const;

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);

		bool createVulkanInstance();

		bool createDebugMessenger();

		bool createSurface();

		bool selectPhysicalDevice();

		bool physicalDeviceSuitable(VkPhysicalDevice device);

		QueueFamilies getQueueFamilies(VkPhysicalDevice device);

		bool createLogicalDevice();

		bool createCommandPool();

		GraphicsDevice(const GraphicsDevice&) = delete;
		GraphicsDevice& operator=(const GraphicsDevice&) = delete;
	};
}
