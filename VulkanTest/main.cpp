#include "__Test__/Rendering/Renderer.h"
#include "__Test__/Rendering/RasterizedMesh.h"
#include "__Test__/Rendering/RayTracedMesh.h"
#include "__Test__/Helpers.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>

namespace {
	/**
	 Most classes can optionally log errors;
	 We pass this function selectively to enable logging.
	 @param text Message to log.
	 */
	static void log(const char* text) {
		std::cout << "<LOG> " << text << std::endl;
	}

	/**
	 * Render loop catches render loop events from the window and invokes necessary calls to render images.
	 */
	class RenderLoop {
	private:
		const std::shared_ptr<Test::SwapChain> m_swapChain;
		const std::vector<std::shared_ptr<Test::Renderer> > m_renderers;
		const std::shared_ptr<Test::VPTransform> m_projection;
		std::chrono::system_clock::time_point m_startDate;
		std::chrono::system_clock::time_point m_lastUpdateDate;
		float m_smoothFPS;
		uint32_t rendererId;

	public:
		/** 
		Constructor for render loop callback (nothing fancy, just takes in the renderers and a few more things it needs to function)
		@param swapChain Our main swap chain (exposes most of what's needed for the internal logic).
		@param transform View-Projection transformation reference.
		@param renderers Target renderers to loop over by pressing space (in our case, there will be two: mesh rasterizer and ray-tracer).
		*/
		template<typename... Renderers>
		RenderLoop(const std::shared_ptr<Test::SwapChain>& swapChain, const std::shared_ptr<Test::VPTransform>& transform, Renderers... renderers)
			: m_swapChain(swapChain), m_renderers({ renderers... }), m_projection(transform)
			, m_startDate(std::chrono::system_clock::now()), m_lastUpdateDate(m_startDate), m_smoothFPS(0.0f), rendererId(0) { }


		/**
		Once the render loop event is registered, window will invoke this function on every iteration of the render loop.
		@param window Reference to the window that invoked the callback.
		*/
		void renderLoopEvent(Test::Window* window) {
			// Issue command to render the image:
			if (m_renderers.size() > 0)
				m_renderers[rendererId]->render();

			std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

			// Updating frame rate counter:
			{
				std::chrono::duration<float> diff = now - m_lastUpdateDate;
				m_lastUpdateDate = now;
				float framerate = 1.0f / diff.count();
				// When we have single digit framerate, somtimes we have a randomness in iteration time, that will show unrealistically high values,
				// so we are biased towards lower...
				float lerpFactor = std::min(diff.count() * 5.0f, (m_smoothFPS < framerate) ? 0.125f : 1.0f);
				m_smoothFPS = ((m_smoothFPS * (1.0f - lerpFactor)) + (framerate * lerpFactor));
				std::stringstream stream;
				stream << "FPS: {smooth:" << m_smoothFPS << "; real:" << framerate << "}";
				window->setTitle(stream.str().c_str());
			}

			// Updating camera position and orientation:
			{
				std::chrono::duration<float> time = now - m_startDate;
				m_projection->view = glm::rotate(
					glm::lookAt(glm::vec3(0.0f, -4.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f))
					, -time.count() * 0.2f, glm::vec3(0.0f, 0.0f, 1.0f)); ;
				VkExtent2D size = m_swapChain->size();

				// Is calculating projection matrix every single frame a good idea? I don't think it is... 
				// We should have a camera class with position, euler angles and perspective field of view that acts as it's own thing.
				m_projection->projection = glm::perspective(glm::radians(60.0f), size.width / (float)size.height, 0.1f, 100.0f);
				m_projection->projection[1][1] *= -1;
			}

			// Switching the renderer if space was pressed (RT mode is slow enough for the system to be less responsive than desirable, 
			// so you may need to hold it for for a few seconds to switch back to rasterized mode):
			if (window->spaceTapped())
				rendererId = (rendererId + 1) % m_renderers.size();
		}
	};
}


int main() {
	/* Note: Used shared pointers all over the place to avoid to have to care about the destruction order... */

	// Window to draw on (non-resizable; resize support is not currently implemented):
	std::shared_ptr<Test::Window> window(new Test::Window("Window", 1280, 720, true, true));
	if (window->closed()) return 1;

	// Graphics device for managing physical and logical device instances:
	std::shared_ptr<Test::GraphicsDevice> device(new Test::GraphicsDevice(window, log));
	if (!device->initialized()) return 2;

	// Swap chain, responsible for managing frame buffers:
	std::shared_ptr<Test::SwapChain> swapChain(new Test::SwapChain(device, log));
	if (!swapChain->initialized()) return 3;

	// Defining scene geometry by reading geometry from the file and appending the plane to it:
	std::vector<Test::PNCVertex> vertices; 
	std::vector<uint32_t> indices;
	loadObj("unit-sphere.obj", vertices, indices, glm::vec3{1.0f, 1.0f, 1.0f}, log);
	// Appending the plane to the scene:
	{
		const std::vector<Test::PNCVertex> PLANE_VERTS = {
			{{-2.0f, -2.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
			{{-2.0f, 2.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
			{{2.0f, -2.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
			{{2.0f, 2.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}
		};
		const std::vector<uint32_t> PLANE_INDICES = {
			0, 2, 1, 
			2, 3, 1
		};
		uint32_t baseIndex = (uint32_t)vertices.size();
		for (size_t i = 0; i < PLANE_VERTS.size(); i++)
			vertices.push_back(PLANE_VERTS[i]);
		for (size_t i = 0; i < PLANE_INDICES.size(); i++)
			indices.push_back(PLANE_INDICES[i] + baseIndex);
	}

	// Mesh for holding the scene geometry on the graphics processor memory:
	std::shared_ptr<Test::Mesh> mesh(new Test::Mesh(device, vertices, indices, log));
	if (!mesh->initialized()) return 4;

	// View-Projection transform that acts as our camera:
	std::shared_ptr<Test::VPTransform> transform(new Test::VPTransform());
	std::shared_ptr<Test::PointLight> light(new Test::PointLight{ {-4.0f, 0.0f, 4.0f}, {10.0f, 15.0f, 10.0f}, {0.1f, 0.05f, 0.075f} });

	// Target Object for rasterized mode ("Object" being an abstraction for a renderable item, alongside all the information about camera and lighting):
	std::shared_ptr<Test::IRenderObject> rasterizedMesh(new Test::RasterizedMesh(mesh, transform, light, log));
	if (!rasterizedMesh->initialized()) return 5;

	// Target Object for ray-traced mode:
	std::shared_ptr<Test::IRenderObject> rayTracedMesh(new Test::RayTracedMesh(mesh, transform, light, log));
	if (!rayTracedMesh->initialized()) return 5;

	// Renderer for rasterized mode:
	std::shared_ptr<Test::Renderer> rasterized(new Test::Renderer(device, swapChain, rasterizedMesh, log));
	if (!rasterized->initialized()) return 6;

	// Renderer for ray-traced mode:
	std::shared_ptr<Test::Renderer> rayTraced(new Test::Renderer(device, swapChain, rayTracedMesh, log));
	if (!rayTraced->initialized()) return 7;

	// RenderLoop just makes sure, the image render commands are issued from correct renderers:
	RenderLoop loop(swapChain, transform, rasterized, rayTraced);
	Test::Window::RenderLoopEventId eventId = window->addRenderLoopEvent(std::bind(&RenderLoop::renderLoopEvent, &loop, std::placeholders::_1));

	// In case something fails, window is configured to closed automatically, so we have to wait here:
	window->waitTillClosed();

	// We have to remove render loop event, so that there is a guarantee, the window will not invoke it once the render loop goes out of scope:
	window->removeRenderLoopEvent(eventId);

	return 0;
}
