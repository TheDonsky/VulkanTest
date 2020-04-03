#pragma once
#include "../Api.h"
#include <thread>
#include <string>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <condition_variable>

namespace Test {
	/**
	 * Well... This one is a wrapper on top of another wrapper and does two things:
	 * 0. Makes an additional abstraction layer for us, making it irrelevant, which window library are we using.
	 * 1. Runs the render loop in a separate thread and sends update events to whoever's interested in them.
	 */
	class Window {
	public:
		/**
		Instantiates and opens the window.
		@param windowTitle Title of the window (can be altered post-creation).
		@param windowW Initial width of the window (may change).
		@param windowH Initial height of the window (may change).
		@param resizable If true, the user will be able to resize the window.
		@param autoCloseOnDestroy If true, the window will close if the object goes out of scope (otherwise, destructor will wait for the user to close the window).
		*/
		Window(const char *windowTitle = "Test Window", int windowW = 800, int windowH = 600, bool resizable = false, bool autoCloseOnDestroy = false);

		/** Destructor */
		~Window();

		/**
		Tells if the space key gets pressed (full library would have a complete input module, but this one was enough for our use case).
		@return true, if space key was just pressed (state is updated once per render loop, so if the performance is substandart, we may have to wait for a while for a positive value to come up).
		*/
		bool spaceTapped()const;

		/**
		Current window title.
		@return text at the top of the window.
		*/
		const std::string& title()const;

		/**
		Requests change of the window title.
		@param newTitle Title to set.
		*/
		void setTitle(const char* newTitle);

		/**
		Current width of the window.
		@return horizontal window extent.
		*/
		int width()const;

		/**
		Current height of the window.
		@return vertical window extent.
		*/
		int height()const;

		/**
		Manually requests the window to close.
		*/
		void close();
		
		/**
		Waits for the user to close the window.
		*/
		void waitTillClosed();

		/**
		Tells if the window is closed or still open.
		@return true, once the window is open no more.
		*/
		bool closed()const;


		/** Type definition of a callback that can be attached to the window to be invoked each frame. */
		typedef std::function<void(Window*)> RenderLoopEvent;

		/** Unique identifier for a registered render loop event. */
		typedef size_t RenderLoopEventId;

		/**
		Adds an event to the window to be invoked every single frame.
		@param loopEvent.
		@return unique registration id to be used for undoing registration by calling removeRenderLoopEvent().
		*/
		RenderLoopEventId addRenderLoopEvent(const RenderLoopEvent& loopEvent);

		/**
		Removes render loop event registration.
		@param eventId Unique identifier of the registration, previously returned by addRenderLoopEvent().
		*/
		void removeRenderLoopEvent(RenderLoopEventId eventId);

		
		/**
		Creates Vulkan surface for the window.
		@param instance Vulkan instance.
		@return newly created surface, tied to the window, till it gets closed.
		*/
		VkSurfaceKHR createSurface(VkInstance instance);

		/**
		Retrieves the list of the required Vulkan instance level extensions.
		@param count Reference to store the number of extensions at.
		@param extensions Reference to store the extension list's address at.
		*/
		static void getRequiredInstanceExtensions(uint32_t& count, const char**& extensions);





	private:
		static void runRenderThread(Window* loop, std::condition_variable* ready);

		void renderThread(std::condition_variable* ready);

		bool createWindow();

		void destroyWindow();

		void open();

		void render();

		volatile int m_width, m_height;

		bool m_resizable;

		bool m_closeOnDestroy;

		std::string m_title;

		GLFWwindow* m_window;

		std::mutex m_windowLock;

		volatile bool m_shouldClose;

		volatile bool m_closed;

		std::mutex m_joinLock;

		std::thread m_renderThread;

		RenderLoopEventId m_renderLoopEventCounter;
		std::mutex m_renderLoopEventLock;
		std::unordered_map<RenderLoopEventId, RenderLoopEvent> m_renderLoopEvents;

		volatile uint8_t m_spaceState;

		inline Window(const Window&) = delete;
		inline Window& operator=(const Window&) = delete;
	};
}
