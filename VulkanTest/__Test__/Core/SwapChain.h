#pragma once
#include "GraphicsDevice.h"
#include "../Objects/Buffers.h"

namespace Test {
	/**
	 * Basic wrapper for creating and managing lifecycle of the shared window-related Vulkan dynamic resources 
	 * like the swap chain and it's corresponding depth and frame buffers. 
	 */
	class SwapChain {
	public:
		/**
		Builds a swap chain object.
		@param device GraphicsDevice to base our swap chain on.
		@param logFn Logging function for error reporing.
		*/
		SwapChain(const std::shared_ptr<GraphicsDevice>& device, void(*logFn)(const char*) = nullptr);

		/** Destructor */
		~SwapChain();

		/** 
		 * As it happens to be the case, we may need to destroy the swap chain resources and create new ones if,
		 * for example, the window gets resized or something like that happens. 
		 * Whenever we touch the interanls of the swap chain, there might be some other dependent resources that need to be reallocated as well.
		 * Because of that, I've implemented a way for the swap chain to inform those objects by invoking registered listeners functions
		 * and this is the type definition for a generic callback for that.
		 */
		typedef std::function<void()> RecreationListener;

		/**
		 * When we register a listener, it is saved within the swap chain with an unique identifier, 
		 * thac can be used to remove the listener, in case, for example, the listening object goes out of scope or it's no longer interested in the state of the swap chain.
		 */
		typedef size_t RecreationListenerId;

		/**
		Registers a recreation listener and invokes it, if the swap chain is fully functional.
		@param listener Calllback to be invoked each time the swap chain gets reinitialized.
		@return Unique identifier for the listener registration.
		*/
		RecreationListenerId addRecreationListener(const RecreationListener& listener);

		/**
		HAS TO BE Invoked whenever the listening object goes out of scope, or no longer fancies to receve information about swap chain changes to avoid crashes and/or unexpected behaviour.
		@param listenerId Identifier of the registration to remove.
		*/
		void removeRecreationListener(RecreationListenerId listenerId);

		/**
		Gives us a way to determine if the swap chain is malformed or not.
		@return true, if swap chain object is fully functional.
		*/
		bool initialized()const;

		/**
		Current dimensions of the frame buffers.
		@return frame buffer width and height.
		*/
		const VkExtent2D& size()const;

		/**
		Surface format of the frame buffers.
		@return pixel format.
		*/
		const VkSurfaceFormatKHR& format()const;

		/**
		Render pass that can operate on this swap chain.
		Note: The instance can be reused, therefore, I decided to keep it here instead of the renderer itself.
		@return render pass.
		*/
		VkRenderPass renderPass()const;

		/**
		Amount of frame buffers within the swap chain.
		@return number of images.
		*/
		uint32_t frameBufferCount()const;

		/**
		Gives access to swap chain frame buffer by index.
		@return frame buffer for color attachment.
		*/
		VkFramebuffer frameBuffer(size_t index)const;

		/**
		Depth buffer.
		@return image for depth attachment.
		*/
		Image& depthBuffer();

		/**
		Gets an avalable image from the frame buffer.
		@param index Index of the aquired frame buffer will be written to this value.
		@param semaphoreToWait Semaphore to be used when enquing draw call in graphics queue (use this in VkPipelineStageFlags).
		@param renderSemaphore Semaphore that should be signalled once the draw call is complete (use this in VkPipelineStageFlags).
		@return false, if image could not be aquired.
		*/
		bool aquireNextImage(size_t& index, VkSemaphore*& semaphoreToWait, VkSemaphore*& renderSemaphore);

		/**
		Requests to display image on screen.
		@param index Frame buffer index within the swap chain (usually the same as the one returned by the last aquireNextImage() call).
		*/
		void present(size_t index);


	public:
		/** 
		 * Information about swap chain capabilities.
		 */
		struct SwapChainSupportInfo {
			// Swap chain capabilities.
			VkSurfaceCapabilitiesKHR capabilities;

			// List of available pixel formats for color attachment.
			std::vector<VkSurfaceFormatKHR> pixelFormats;

			// List of supported present modes.
			std::vector<VkPresentModeKHR> presentModes;

			/** Default constructor (removed a warning... Don't mind this one) */
			inline SwapChainSupportInfo() : capabilities{} {}
		};

		/**
		Retrieves information about swap chain capabilities of the given physical device for a specific surface.
		@param device Physical device.
		@param surface Window surface.
		@return infomration about swap chain capabilities.
		*/
		static SwapChainSupportInfo getSwapChainSupportInfo(VkPhysicalDevice device, VkSurfaceKHR surface);





	private:
		const std::shared_ptr<GraphicsDevice> m_device;

		VkSwapchainKHR m_swapChain;

		VkRenderPass m_renderPass;

		std::vector<VkImage> m_images;

		std::vector<VkImageView> m_imageViews;

		std::vector<VkFramebuffer> m_frameBuffers;

		VkSurfaceFormatKHR m_pixelFormat;

		VkExtent2D m_size;

		std::unique_ptr<Image> m_depthBuffer;

		VkSemaphore m_imageAvailable;
		VkSemaphore m_renderFinished;

		bool m_initialized;

		RecreationListenerId m_recreationListenerIdCounter;
		mutable std::mutex m_recreationLock;
		typedef std::unordered_map<RecreationListenerId, RecreationListener> RecreationListeners;
		RecreationListeners m_recreationListeners;


		void(*m_logFn)(const char*);

		void log(const char* message)const;

		bool createSwapChain();

		void fetchImages();

		void clearImageViews();

		bool createImageViews();

		bool createDepthBuffer();

		bool createRenderPass();

		void clearFrameBuffers();

		bool createFrameBuffers();

		void recreateSwapChain();

		void clearSwapChain();

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;
	};
}
