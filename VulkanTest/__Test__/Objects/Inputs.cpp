#include "Inputs.h"

namespace {
	inline static VkVertexInputBindingDescription VertexBindingDescription() {
		VkVertexInputBindingDescription desc = {};
		desc.binding = 0;
		desc.stride = sizeof(Test::PNCVertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}
	static const VkVertexInputBindingDescription BINDING_DESCRIPTION = VertexBindingDescription();

	inline static std::vector<VkVertexInputAttributeDescription> VertexAttributeDescription() {
		std::vector<VkVertexInputAttributeDescription> desc(3);
		// position:
		{
			VkVertexInputAttributeDescription& position = desc[0];
			position = {};
			position.binding = 0;
			position.location = 0;
			position.format = VK_FORMAT_R32G32B32_SFLOAT;
			position.offset = offsetof(Test::PNCVertex, position);
		}
		// normal:
		{
			VkVertexInputAttributeDescription& normal = desc[1];
			normal = {};
			normal.binding = 0;
			normal.location = 1;
			normal.format = VK_FORMAT_R32G32B32_SFLOAT;
			normal.offset = offsetof(Test::PNCVertex, normal);
		}
		// color:
		{
			VkVertexInputAttributeDescription& color = desc[2];
			color = {};
			color.binding = 0;
			color.location = 2;
			color.format = VK_FORMAT_R32G32B32_SFLOAT;
			color.offset = offsetof(Test::PNCVertex, color);
		}
		return desc;
	}
	static const std::vector<VkVertexInputAttributeDescription> ATTRIBUTE_DESCRIPTION = VertexAttributeDescription();
}

namespace Test {
	const VkVertexInputBindingDescription& PNCVertex::bindingDescription() { return BINDING_DESCRIPTION; }

	const std::vector<VkVertexInputAttributeDescription>& PNCVertex::attributeDescription() { return ATTRIBUTE_DESCRIPTION; }
}

