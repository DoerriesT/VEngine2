#include "WavefrontOBJLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <EASTL/sort.h>
#include <EASTL/set.h>
#include <EASTL/hash_map.h>
#include <EASTL/hash_set.h>
#include <glm/geometric.hpp>
#include <filesystem>
#include <Log.h>

//namespace
//{
//	struct Vertex
//	{
//		glm::vec3 position;
//		glm::vec3 normal;
//		glm::vec2 texCoord;
//	};
//
//	inline bool operator==(const Vertex &lhs, const Vertex &rhs)
//	{
//		return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
//	}
//
//	template <class T>
//	inline void hashCombine(size_t &s, const T &v)
//	{
//		eastl::hash<T> h;
//		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
//	}
//
//	struct VertexHash
//	{
//		inline size_t operator()(const Vertex &value) const
//		{
//			size_t hashValue = 0;
//
//			for (size_t i = 0; i < sizeof(Vertex); ++i)
//			{
//				hashCombine(hashValue, reinterpret_cast<const char *>(&value)[i]);
//			}
//
//			return hashValue;
//		}
//	};
//}

bool WavefrontOBJLoader::loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes, bool importSkeletons, bool importAnimations, float scale, LoadedModel &model)
{
	model = {};

	if (!importMeshes)
	{
		return true;
	}

	// load scene
	tinyobj::attrib_t objAttrib;
	std::vector<tinyobj::shape_t> objShapes;
	std::vector<tinyobj::material_t> objMaterials;

	std::string warn;
	std::string err;
	std::filesystem::path baseDir(filepath);
	baseDir.remove_filename();

	bool ret = tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &warn, &err, filepath, baseDir.u8string().c_str(), true);

	if (!warn.empty())
	{
		Log::warn("WavefrontOBJLoader: Warning while loading file \"%s\": %s", filepath, warn.c_str());
	}

	if (!err.empty())
	{
		Log::err("WavefrontOBJLoader: Error while loading file \"%s\": %s", filepath, err.c_str());
	}

	if (!ret)
	{
		Log::err("WavefrontOBJLoader: Failed to load file \"%s\"!", filepath);
		return false;
	}

	// create materials
	{
		for (const auto &objMaterial : objMaterials)
		{
			LoadedMaterial mat{};
			mat.m_name = objMaterial.name.c_str();
			mat.m_alpha = LoadedMaterial::Alpha::OPAQUE;
			mat.m_albedoFactor = { objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2] };
			mat.m_metalnessFactor = objMaterial.metallic;
			mat.m_roughnessFactor = objMaterial.roughness;
			mat.m_emissiveFactor = { objMaterial.emission[0], objMaterial.emission[1], objMaterial.emission[2] };
			mat.m_opacity = 1.0f;
			mat.m_albedoTexture = objMaterial.diffuse_texname.c_str();
			mat.m_normalTexture = objMaterial.normal_texname.empty() ? objMaterial.bump_texname.c_str() : objMaterial.normal_texname.c_str();
			mat.m_metalnessTexture = objMaterial.metallic_texname.c_str();
			mat.m_roughnessTexture = objMaterial.roughness_texname.c_str();
			mat.m_occlusionTexture = "";
			mat.m_emissiveTexture = objMaterial.emissive_texname.c_str();
			mat.m_displacementTexture = objMaterial.displacement_texname.c_str();

			model.m_materials.push_back(mat);
		}
	}

	struct Index
	{
		uint32_t m_shapeIndex;
		uint32_t m_materialIndex;
		tinyobj::index_t m_vertexIndices[3];
	};

	eastl::vector<Index> unifiedIndices;

	for (size_t shapeIndex = 0; shapeIndex < objShapes.size(); ++shapeIndex)
	{
		const auto &shape = objShapes[shapeIndex];
		for (size_t index = 0; index < shape.mesh.indices.size(); index += 3)
		{
			Index unifiedIndex;
			unifiedIndex.m_shapeIndex = static_cast<uint32_t>(shapeIndex);
			unifiedIndex.m_materialIndex = shape.mesh.material_ids[index / 3];
			unifiedIndex.m_vertexIndices[0] = shape.mesh.indices[index + 0];
			unifiedIndex.m_vertexIndices[1] = shape.mesh.indices[index + 1];
			unifiedIndex.m_vertexIndices[2] = shape.mesh.indices[index + 2];

			unifiedIndices.push_back(unifiedIndex);
		}
	}

	if (mergeByMaterial)
	{
		// sort all by material
		eastl::stable_sort(unifiedIndices.begin(), unifiedIndices.end(), [](const auto &lhs, const auto &rhs) { return lhs.m_materialIndex < rhs.m_materialIndex; });
	}
	else
	{
		// sort only inside same shape by material
		eastl::stable_sort(unifiedIndices.begin(), unifiedIndices.end(),
			[](const auto &lhs, const auto &rhs)
			{
				return lhs.m_shapeIndex < rhs.m_shapeIndex || (lhs.m_shapeIndex == rhs.m_shapeIndex && lhs.m_materialIndex < rhs.m_materialIndex);
			});
	}

	model.m_aabbMin = glm::vec3(FLT_MAX);
	model.m_aabbMax = glm::vec3(-FLT_MAX);

	eastl::set<uint32_t> shapeIndices;

	LoadedModel::Mesh mesh = {};

	// loop over all primitives and create individual meshes
	for (size_t i = 0; i < unifiedIndices.size(); ++i)
	{
		const auto &unifiedIdx = unifiedIndices[i];
		// loop over face (triangle)
		for (size_t j = 0; j < 3; ++j)
		{
			glm::vec3 position;
			glm::vec3 normal;
			glm::vec2 texCoord;

			const auto &vertexIndex = unifiedIdx.m_vertexIndices[j];

			if (vertexIndex.vertex_index == -1)
			{
				Log::err("WavefrontOBJLoader: Vertices of file \"%s\" are missing positions!", filepath);
				return false;
			}
			if (vertexIndex.normal_index == -1)
			{
				Log::err("WavefrontOBJLoader: Vertices of file \"%s\" are missing normals!", filepath);
				return false;
			}
			if (vertexIndex.texcoord_index == -1)
			{
				Log::err("WavefrontOBJLoader: Vertices of file \"%s\" are missing UVs!", filepath);
				return false;
			}

			position.x = objAttrib.vertices[vertexIndex.vertex_index * 3 + 0];
			position.y = objAttrib.vertices[vertexIndex.vertex_index * 3 + 1];
			position.z = objAttrib.vertices[vertexIndex.vertex_index * 3 + 2];
			position *= scale;

			normal.x = objAttrib.normals[vertexIndex.normal_index * 3 + 0];
			normal.y = objAttrib.normals[vertexIndex.normal_index * 3 + 1];
			normal.z = objAttrib.normals[vertexIndex.normal_index * 3 + 2];

			if (objAttrib.texcoords.size() > vertexIndex.texcoord_index * 2 + 1)
			{
				texCoord.x = objAttrib.texcoords[vertexIndex.texcoord_index * 2 + 0];
				texCoord.y = objAttrib.texcoords[vertexIndex.texcoord_index * 2 + 1];

				if (invertTexcoordY)
				{
					texCoord.y = 1.0f - texCoord.y;
				}
			}
			else
			{
				texCoord.x = 0.0f;
				texCoord.y = 0.0f;
			}

			mesh.m_aabbMin = glm::min(mesh.m_aabbMin, position);
			mesh.m_aabbMax = glm::max(mesh.m_aabbMax, position);

			mesh.m_positions.push_back(position);
			mesh.m_normals.push_back(normal);
			mesh.m_texCoords.push_back(texCoord);

			shapeIndices.insert(unifiedIdx.m_shapeIndex);
		}

		if (i + 1 == unifiedIndices.size()														// last index -> we're done
			|| unifiedIndices[i + 1].m_materialIndex != unifiedIdx.m_materialIndex					// next material is different -> create new mesh
			|| (!mergeByMaterial && unifiedIndices[i + 1].m_shapeIndex != unifiedIdx.m_shapeIndex))	// dont merge by material (keep distinct objects) and next shape is different -> create new mesh
		{
			for (const auto shapeIndex : shapeIndices)
			{
				if (!mesh.m_name.empty())
				{
					mesh.m_name += "_";
				}
				mesh.m_name += objShapes[shapeIndex].name.c_str();
			}

			mesh.m_materialIndex = unifiedIdx.m_materialIndex;

			// allocate space for tangents
			mesh.m_tangents.resize(mesh.m_normals.size());

			model.m_aabbMin = glm::min(mesh.m_aabbMin, model.m_aabbMin);
			model.m_aabbMax = glm::max(mesh.m_aabbMax, model.m_aabbMax);

			model.m_meshes.push_back(eastl::move(mesh));

			// reset temp data
			mesh = {};
			shapeIndices.clear();
		}
	}

	return true;
}