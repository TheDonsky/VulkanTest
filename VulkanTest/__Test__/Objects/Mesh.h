#pragma once
#include "Buffers.h"
#include "Inputs.h"

namespace Test {
	/**
	 * Stores geometry of a mesh/scene on the memory of a graphics processor.
	 */
	class Mesh {
	public:
		/**
		Instantiates a mesh object on GPU.
		@param device Graphics device reference.
		@param verts Data for vertex buffer.
		@param indexBuffer Data for index buffer.
		@param logFn Logging function for error reporting (optional).
		*/
		Mesh(const std::shared_ptr<GraphicsDevice>& device, const std::vector<PNCVertex>& verts, const std::vector<uint32_t> indexBuffer, void(*logFn)(const char*) = nullptr);

		/**
		Shared pointer to the graphics device, the mesh is located on.
		@return graphics device.
		*/
		const std::shared_ptr<GraphicsDevice>& device()const;

		/**
		Tells, if the mesh is well-formed or not.
		@return false if any of the underlying buffers failed to be instantiated.
		*/
		bool initialized()const;

		/**
		Number of vertices within the mesh.
		@return amount of element s within the vertex buffer.
		*/
		uint32_t numVertices()const;

		/**
		Buffer, containing all the mesh vertices.
		@return vertex buffer.
		*/
		VkBuffer vertexBuffer()const;

		/**
		Number of indices within the mesh (3 times the polycount).
		@return amount of element s within the index buffer.
		*/
		uint32_t numIndices()const;

		/**
		Buffer, containing triples of triangle vertex indices.
		@return index buffer.
		*/
		VkBuffer indexBuffer()const;





	private:
		const std::shared_ptr<GraphicsDevice> m_graphicsDevice;
		VertexBuffer<PNCVertex> m_vertexBuffer;
		IndexBuffer m_indexBuffer;
	};
}

