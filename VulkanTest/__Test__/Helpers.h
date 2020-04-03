#pragma once
#include "Api.h"
#include <fstream>
#include <vector>
#include "Objects/Inputs.h"
#include "../__ThirdParty__/TinyObjLoader/tiny_obj_loader.h"

/**
 * Here we have a few inline helpers for reading files, compiling shaders and building meshes.
 */
namespace Test {
	/**
	Reads binary file content into a vector of characters.
	@param filename Name of the file to read (can be either relative or absolute path).
	@param content Vector to fill with the file's content.
	@return true upon success.
	*/
	inline static bool readFile(const char* filename, std::vector<char> &content) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) return false;
		content.resize(file.tellg());
		file.seekg(0);
		file.read(content.data(), content.size());
		file.close();
		return true;
	}

	/**
	Reads shader file and builds a shader module from it.
	@param device Logical Vulkan device.
	@param filename SPV file name (can be either relative or absolute path).
	@param pModule Reference to the shader module handle to store the result.
	@return true, if the shader module gets initialized without issues.
	*/
	inline static bool createShaderModule(VkDevice device, const char* filename, VkShaderModule* pModule) {
		std::vector<char> content;
		if (!readFile(filename, content)) return false;
		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = content.size();
		info.pCode = reinterpret_cast<const uint32_t*>(content.data());
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &info, nullptr, &shaderModule) != VK_SUCCESS) return false;
		(*pModule) = shaderModule;
		return true;
	}

	/**
	Load wavefront .obj file and appends it's content to given geometry storage.
	@param filename Name of the .obj file to read (as always, can be either relative or absolute path).
	@param vertices Vertices from the file will be appended to this vector if the file is parsed successfully.
	@param indices Indices from the file will be appended to this vector if the faile is parsed successfully.
	@param color All the vertices from this file will be assigned this color.
	@param logFn If there are any read errors/warnings, this function will be used to report the cause (optional; can be null).
	@return true, if there are no parsing errors.
	*/
	inline static bool loadObj(const char* filename,std::vector<Test::PNCVertex>& vertices, std::vector<uint32_t>& indices, const glm::vec3& color, void(*logFn)(const char*)) {
		tinyobj::attrib_t attributes;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string warning;
		std::string error;

		bool loaded = tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, filename);

		if (!warning.empty()) if (logFn != nullptr) logFn(warning.c_str());

		if (!error.empty()) if (logFn != nullptr) logFn(error.c_str());

		if (!loaded) return false;

		struct Id {
			int vertex;
			int normal;

			inline Id() : vertex(-1), normal(-1) {};
			inline Id(const tinyobj::index_t& id) : vertex(id.vertex_index), normal(id.normal_index) {}
			inline bool operator<(const Id& other)const { return (vertex < other.vertex) || ((vertex == other.vertex) && normal < other.normal); }
		};

		std::map<Id, uint32_t> indexCache;
		std::vector<uint32_t> vertexId;
		uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

		for (size_t shapeId = 0; shapeId < shapes.size(); shapeId++) {
			const tinyobj::mesh_t& mesh = shapes[shapeId].mesh;
			size_t index = 0;
			for (size_t faceId = 0; faceId < mesh.num_face_vertices.size(); faceId++) {
				size_t faceStart = (uint32_t)vertexId.size();
				size_t vertCount = mesh.num_face_vertices[faceId];
				for (size_t i = 0; i < vertCount; i++) {
					const Id idx = mesh.indices[index + i];
					std::map<Id, uint32_t>::iterator it = indexCache.find(idx);
					if (it == indexCache.end()) {
						size_t vertexX = ((size_t)idx.vertex * 3);
						size_t normalX = ((size_t)idx.normal * 3);
						Test::PNCVertex vertex = {
							{attributes.vertices[vertexX], attributes.vertices[vertexX + 1], attributes.vertices[vertexX + 2]},
							{attributes.normals[normalX], attributes.normals[normalX + 1], attributes.normals[normalX + 2]},
							{color.x, color.y, color.z}
						};
						vertexId.push_back(static_cast<uint32_t>(vertices.size()));
						vertices.push_back(vertex);
					}
					else vertexId.push_back(it->second);
					if (i >= 2) {
						indices.push_back(vertexId[faceStart]);
						indices.push_back(vertexId[vertexId.size() - 2]);
						indices.push_back(vertexId[vertexId.size() - 1]);
					}
				}
				index += vertCount;
			}
		}
		return true;
	}
}
