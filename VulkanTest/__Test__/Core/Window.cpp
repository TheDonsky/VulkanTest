#include "Window.h"

namespace {
	// This guy is something, we are gonna use each time we need to do something with glfw, to make sure, 
	// the library gets initialized only when needed and terminated, once the apllication is ready to close.
	class Helper {
	private:
		Helper() { glfwInit(); }
		~Helper() { glfwTerminate(); }


	public:
		static const Helper& instance() {
			static Helper inst;
			return inst;
		}

		void hint(int hint, int value)const { glfwWindowHint(hint, value); }
		
		GLFWwindow* createWindow(int width, int height, const std::string &title)const { 
			return glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
		}

		void destroyWindow(GLFWwindow* window)const {
			glfwDestroyWindow(window);
		}

		void getRequiredInstanceExtensions(uint32_t& count, const char**& extensions)const {
			extensions = glfwGetRequiredInstanceExtensions(&count);
		}
	};


	enum class KeyState : uint8_t {
		RELEASED,
		DOWN,
		PRESSED,
		UP
	};

	void updateKeyState(volatile uint8_t& state, GLFWwindow* window, int key) {
		if (glfwGetKey(window, key) == GLFW_PRESS) {
			if (state == (uint8_t)KeyState::RELEASED || state == (uint8_t)KeyState::UP) 
				state = (uint8_t)KeyState::DOWN;
			else if (state == (uint8_t)KeyState::DOWN) 
				state = (uint8_t)KeyState::PRESSED;
		}
		else if (state == (uint8_t)KeyState::UP) 
			state = (uint8_t)KeyState::RELEASED;
		else if (state == (uint8_t)KeyState::DOWN || state == (uint8_t)KeyState::PRESSED) 
			state = (uint8_t)KeyState::UP;
	}
}

namespace Test {
	Window::Window(const char *title, int windowW, int windowH, bool resizable, bool autoCloseOnDestroy)
		: m_width(windowW), m_height(windowH), m_resizable(resizable), m_closeOnDestroy(autoCloseOnDestroy)
		, m_title(title == nullptr ? "" : title), m_window(nullptr), m_shouldClose(false), m_closed(true), m_renderLoopEventCounter(0)
		, m_spaceState((uint8_t)KeyState::RELEASED) {
		open();
	}

	Window::~Window() {
		if (m_closeOnDestroy) close();
		else waitTillClosed();
	}

	bool Window::spaceTapped()const {
		return m_spaceState == (uint8_t)KeyState::DOWN;
	}

	const std::string& Window::title()const { return m_title; }

	void Window::setTitle(const char *newTitle) {
		std::unique_lock<std::mutex> lock(m_windowLock);
		m_title = newTitle;
		if (closed() || m_window == nullptr) return;
		glfwSetWindowTitle(m_window, newTitle);
	}

	int Window::width()const { 
		return m_width; 
	}

	int Window::height()const { 
		return m_height; 
	}

	void Window::open() {
		std::unique_lock<std::mutex> joinLock(m_joinLock);
		if (m_renderThread.joinable()) return;
		m_closed = false;
		std::mutex mutex;
		std::unique_lock<std::mutex> lock(mutex);
		std::condition_variable ready;
		m_renderThread = std::thread(runRenderThread, this, &ready);
		ready.wait(lock);
	}

	void Window::close() {
		std::unique_lock<std::mutex> lock(m_joinLock);
		m_shouldClose = true;
		if (m_renderThread.joinable())
			m_renderThread.join();
		m_shouldClose = false;
	}

	void Window::waitTillClosed() {
		std::unique_lock<std::mutex> lock(m_joinLock);
		if (m_renderThread.joinable())
			m_renderThread.join();
	}

	bool Window::closed()const { return m_closed; }

	Window::RenderLoopEventId Window::addRenderLoopEvent(const RenderLoopEvent& loopEvent) {
		std::unique_lock<std::mutex> lock(m_renderLoopEventLock);
		while (m_renderLoopEvents.find(m_renderLoopEventCounter) != m_renderLoopEvents.end()) 
			m_renderLoopEventCounter++;
		RenderLoopEventId id = m_renderLoopEventCounter;
		m_renderLoopEvents[id] = loopEvent;
		m_renderLoopEventCounter++; // This kinda sorta guarantees, that 'while' loop above will never go past a single iteration during the lifetime of the universe.
		return id;
	}

	void Window::removeRenderLoopEvent(RenderLoopEventId eventId) {
		std::unique_lock<std::mutex> lock(m_renderLoopEventLock);
		std::unordered_map<RenderLoopEventId, RenderLoopEvent>::iterator it = m_renderLoopEvents.find(eventId);
		if (it != m_renderLoopEvents.end())
			m_renderLoopEvents.erase(it);
	}

	VkSurfaceKHR Window::createSurface(VkInstance instance) {
		std::unique_lock<std::mutex> lock(m_windowLock);
		if (closed()) return nullptr;
		VkSurfaceKHR surface;
		if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != VK_SUCCESS)
			return VK_NULL_HANDLE;
		else return surface;
	}

	void Window::getRequiredInstanceExtensions(uint32_t& count, const char**& extensions) {
		Helper::instance().getRequiredInstanceExtensions(count, extensions);
	}


	void Window::runRenderThread(Window* loop, std::condition_variable* ready) { loop->renderThread(ready); }

	void Window::renderThread(std::condition_variable* ready) {
		if (createWindow()) {
			ready->notify_one();
			while ((!glfwWindowShouldClose(m_window)) && (!m_shouldClose)) {
				glfwPollEvents();
				{
					bool shouldClose = false;
					int width = 0, height = 0;
					while (width == 0 || height == 0) {
						glfwGetFramebufferSize(m_window, &width, &height);
						if (glfwWindowShouldClose(m_window)) {
							shouldClose = true;
							break;
						}
					}
					if (shouldClose) break;
					m_width = width;
					m_height = height;
				}
				updateKeyState(m_spaceState, m_window, GLFW_KEY_SPACE); // Probably should build a better system for buttons, but this will suffice for test.
				render();
			}
			destroyWindow();
			{
				std::unique_lock<std::mutex> lock(m_windowLock);
				m_closed = true;
			}
		}
		else {
			std::unique_lock<std::mutex> lock(m_windowLock);
			m_closed = true;
			ready->notify_one();
		}
	}

	bool Window::createWindow() {
		std::unique_lock<std::mutex> lock(m_windowLock);
		if (m_window != nullptr) return true;
		Helper::instance().hint(GLFW_CLIENT_API, GLFW_NO_API);
		Helper::instance().hint(GLFW_RESIZABLE, m_resizable ? GLFW_TRUE : GLFW_FALSE);
		m_window = Helper::instance().createWindow(m_width, m_height, m_title);
		return (m_window != nullptr);
	}

	void Window::destroyWindow() {
		std::unique_lock<std::mutex> lock(m_windowLock);
		if (m_window == nullptr) return;
		Helper::instance().destroyWindow(m_window);
		m_window = nullptr;
	}

	void Window::render() {
		std::unique_lock<std::mutex> lock(m_renderLoopEventLock);
		// Yep, I know, I know... Iterating over a hashmap may be slightly substandard when it comes to the performance of a realtime application, 
		// but nobody expects this collection to hold millions of items and, anyway, the content of the callbacks is expected to be much heavier than this overhead, so... 
		// I think this might just be OK-ish for our case.
		for (std::unordered_map<RenderLoopEventId, RenderLoopEvent>::const_iterator it = m_renderLoopEvents.begin(); it != m_renderLoopEvents.end(); ++it)
			it->second(this);
	}
}


