#pragma once
#include "../Api.h"
#include <vector>

namespace Test {
	/**
	 * Vertex, containing position, normal and vertex color information.
	 */
	struct PNCVertex {
		alignas(16) glm::vec3 position;
		alignas(16) glm::vec3 normal;
		alignas(16) glm::vec3 color;

		/**
		Gets binding description for vertex buffer.
		@return description.
		*/
		static const VkVertexInputBindingDescription& bindingDescription();

		/**
		Gets attribute descriptions for the vertex buffer.
		@return list of attribute descriptions.
		*/
		static const std::vector<VkVertexInputAttributeDescription>& attributeDescription();
	};

	/**
	 * View-Projection transform.
	 */
	struct VPTransform {
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
	};

	/**
	 * Description of a point light.
	 */
	struct PointLight {
		alignas(16) glm::vec3 position;

		// Values here are allowed to be arbitrary for proper distance attenuation purposes.
		alignas(16) glm::vec3 color;

		// Wherever we have a shadow, this will prevent the surface from going full on black (nothing to do with the physics, but we all love ambient, don't we?)
		alignas(16) glm::vec3 ambientStrength;
	};
}

