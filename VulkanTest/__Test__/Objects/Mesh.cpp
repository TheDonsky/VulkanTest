#include "Mesh.h"

namespace Test {
	Mesh::Mesh(const std::shared_ptr<GraphicsDevice>& device, const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, void(*logFn)(const char*))
		: m_graphicsDevice(device)
		, m_vertexBuffer(m_graphicsDevice, static_cast<uint32_t>(verts.size()), verts.data(), logFn)
		, m_indexBuffer(m_graphicsDevice, static_cast<uint32_t>(indexBuffer.size()), indexBuffer.data(), logFn) {}

	const std::shared_ptr<GraphicsDevice>& Mesh::device()const {
		return m_graphicsDevice;
	}

	bool Mesh::initialized()const {
		return m_graphicsDevice != nullptr && m_graphicsDevice->initialized() 
			&& m_vertexBuffer.buffer() != VK_NULL_HANDLE && m_indexBuffer.buffer() != VK_NULL_HANDLE;
	}

	uint32_t Mesh::numVertices()const {
		return m_vertexBuffer.size();
	}

	VkBuffer Mesh::vertexBuffer()const {
		return m_vertexBuffer.buffer();
	}

	uint32_t Mesh::numIndices()const {
		return m_indexBuffer.size();
	}

	VkBuffer Mesh::indexBuffer()const {
		return m_indexBuffer.buffer();
	}
}
