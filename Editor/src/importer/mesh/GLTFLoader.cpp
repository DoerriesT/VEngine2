#include "GLTFLoader.h"
#include <nlohmann/json.hpp>
#include <Log.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE 
#define TINYGLTF_NO_STB_IMAGE_WRITE 
#define TINYGLTF_NO_EXTERNAL_IMAGE 
#define TINYGLTF_NO_INCLUDE_JSON 
#define TINYGLTF_NO_INCLUDE_STB_IMAGE 
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE 
#include <tiny_gltf.h>

#undef OPAQUE

namespace
{
	struct SkeletonInfo
	{
		std::vector<int> jointMap; // maps from node index to actual joint index
		std::vector<bool> isJoint;
	};
}

static float byteToFloat(int8_t b)
{
	return std::max<float>(b / 127.0f, -1.0f);
};

static float ubyteToFloat(uint8_t b)
{
	return b / 255.0f;
};

static float shortToFloat(int16_t b)
{
	return std::max<float>(b / 32767.0f, -1.0f);
};

static float ushortToFloat(uint16_t b)
{
	return b / 65535.0f;
};

static float intToFloat(int32_t b)
{
	return std::max<float>(b / (float)INT32_MAX, -1.0f);
};

static float uintToFloat(uint32_t b)
{
	return b / (float)UINT32_MAX;
};

template<typename T>
static const T *getBufferDataPtr(const tinygltf::Accessor &accessor, const tinygltf::BufferView &bufferView, const tinygltf::Buffer &buffer, size_t componentCount, size_t elementIndex, size_t componentIndex)
{
	const size_t compSize = sizeof(T);
	const size_t stride = bufferView.byteStride == 0 ? componentCount * compSize : bufferView.byteStride;
	return (T *)&buffer.data[accessor.byteOffset + bufferView.byteOffset + elementIndex * stride + componentIndex * compSize];
}

static bool getFloatBufferData(const tinygltf::Model &gltfModel, int accessorIdx, size_t elementIndex, size_t resultSize, float *result)
{
	const tinygltf::Accessor &accessor = gltfModel.accessors[accessorIdx];
	const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
	const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

	size_t componentCount = 1;
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_VEC2:
		componentCount = 2;
		break;
	case TINYGLTF_TYPE_VEC3:
		componentCount = 3;
		break;
	case TINYGLTF_TYPE_VEC4:
		componentCount = 4;
		break;
	case TINYGLTF_TYPE_MAT2:
		componentCount = 4;
		break;
	case TINYGLTF_TYPE_MAT3:
		componentCount = 9;
		break;
	case TINYGLTF_TYPE_MAT4:
		componentCount = 16;
		break;
	case TINYGLTF_TYPE_SCALAR:
		componentCount = 1;
		break;
	default:
		assert(false);
		break;
	}

	if (elementIndex >= accessor.count || resultSize > componentCount)
	{
		return false;
	}

	for (size_t i = 0; i < std::min(resultSize, componentCount); ++i)
	{
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		{
			result[i] = byteToFloat(*getBufferDataPtr<int8_t>(accessor, bufferView, buffer, componentCount, elementIndex, i));
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			result[i] = ubyteToFloat(*getBufferDataPtr<uint8_t>(accessor, bufferView, buffer, componentCount, elementIndex, i));
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			result[i] = shortToFloat(*getBufferDataPtr<int16_t>(accessor, bufferView, buffer, componentCount, elementIndex, i));
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			result[i] = ushortToFloat(*getBufferDataPtr<uint16_t>(accessor, bufferView, buffer, componentCount, elementIndex, i));
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_INT:
		{
			result[i] = intToFloat(*getBufferDataPtr<int32_t>(accessor, bufferView, buffer, componentCount, elementIndex, i));
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			result[i] = uintToFloat(*getBufferDataPtr<int32_t>(accessor, bufferView, buffer, componentCount, elementIndex, i));
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
		{
			result[i] = *getBufferDataPtr<float>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_DOUBLE:
		{
			result[i] = (float)*getBufferDataPtr<double>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		default:
			assert(false);
			break;
		}
	}
	return true;
}

static bool getIntBufferData(const tinygltf::Model &gltfModel, int accessorIdx, size_t elementIndex, size_t resultSize, int64_t *result)
{
	const tinygltf::Accessor &accessor = gltfModel.accessors[accessorIdx];
	const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
	const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

	size_t componentCount = 1;
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_VEC2:
		componentCount = 2;
		break;
	case TINYGLTF_TYPE_VEC3:
		componentCount = 3;
		break;
	case TINYGLTF_TYPE_VEC4:
		componentCount = 4;
		break;
	case TINYGLTF_TYPE_MAT2:
		componentCount = 4;
		break;
	case TINYGLTF_TYPE_MAT3:
		componentCount = 9;
		break;
	case TINYGLTF_TYPE_MAT4:
		componentCount = 16;
		break;
	case TINYGLTF_TYPE_SCALAR:
		componentCount = 1;
		break;
	default:
		assert(false);
		break;
	}

	if (elementIndex >= accessor.count || resultSize > componentCount)
	{
		return false;
	}

	for (size_t i = 0; i < std::min(resultSize, componentCount); ++i)
	{
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_BYTE:
		{
			result[i] = *getBufferDataPtr<int8_t>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			result[i] = *getBufferDataPtr<uint8_t>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_SHORT:
		{
			result[i] = *getBufferDataPtr<int16_t>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			result[i] = *getBufferDataPtr<uint16_t>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_INT:
		{
			result[i] = *getBufferDataPtr<int32_t>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		{
			result[i] = *getBufferDataPtr<int32_t>(accessor, bufferView, buffer, componentCount, elementIndex, i);
			break;
		}
		default:
			assert(false);
			break;
		}
	}
	return true;
}

static bool isJoint(const tinygltf::Model &gltfModel, int node)
{
	if (!gltfModel.skins.empty())
	{
		for (auto joint : gltfModel.skins[0].joints)
		{
			if (joint == node)
			{
				return true;
			}
		}
	}
	return false;
}

static void loadNodes(size_t nodeIdx, const glm::mat4 &parentTransform, const tinygltf::Model &gltfModel, const std::vector<int> &jointMap, bool invertTexcoordY, ImportedModel &resultModel)
{
	const auto &node = gltfModel.nodes[nodeIdx];

	glm::mat4 localTransform = glm::mat4();
	//if (!isJoint(gltfModel, (int)nodeIdx))
	//{
	//	if (!node.matrix.empty())
	//	{
	//		localTransform = (glm::mat4)glm::make_mat4(node.matrix.data());
	//	}
	//	else if (!node.scale.empty() || !node.rotation.empty() || !node.translation.empty())
	//	{
	//		glm::vec3 scale = !node.scale.empty() ? (glm::vec3)glm::make_vec3(node.scale.data()) : glm::vec3(1.0f);
	//		glm::quat rot = !node.rotation.empty() ? (glm::quat)glm::make_quat(node.rotation.data()) : glm::quat();
	//		glm::vec3 trans = !node.translation.empty() ? (glm::vec3)glm::make_vec3(node.translation.data()) : glm::vec3();
	//
	//		localTransform = glm::translate(trans) * glm::mat4_cast(rot) * glm::scale(scale);
	//	}
	//}

	glm::mat4 globalTransform = localTransform * parentTransform;
	glm::mat4 normalTransform = glm::transpose(glm::inverse(globalTransform));

	if (node.mesh != -1)
	{
		const auto &gltfMesh = gltfModel.meshes[node.mesh];

		Log::info(("Importing Mesh: " + gltfMesh.name).c_str());

		for (size_t i = 0; i < gltfMesh.primitives.size(); ++i)
		{
			const auto &primitive = gltfMesh.primitives[i];
			Log::info(("Importing Primitive #" + std::to_string(i)).c_str());

			if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
			{
				Log::warn(("glTF primitive has unsupported primitive mode (" + std::to_string(primitive.mode) + ")! Skipping.").c_str());
				continue;
			}

			const bool indexed = primitive.indices != -1;
			const size_t vertexCount = indexed ? gltfModel.accessors[primitive.indices].count : gltfModel.accessors[primitive.attributes.at("POSITION")].count;

			if (vertexCount % 3 != 0)
			{
				Log::warn(("glTF primitive has a vertex count that is not divisible by 3 (" + std::to_string(vertexCount) + ")! Skipping.").c_str());
				continue;
			}

			const size_t triangleCount = vertexCount / 3;

			int positionsAccessor = -1;
			int normalsAccessor = -1;
			int texCoordsAccessor = -1;
			int weightsAccessor = -1;
			int jointsAccessor = -1;

			// positions accessor
			auto attributeIt = primitive.attributes.find("POSITION");
			if (attributeIt == primitive.attributes.end())
			{
				Log::warn("glTF primitive does not have position attribute! Skipping.");
				continue;
			}

			positionsAccessor = attributeIt->second;


			// normals accessor
			attributeIt = primitive.attributes.find("NORMAL");
			if (attributeIt == primitive.attributes.end())
			{
				Log::warn("glTF primitive does not have normal attribute! Skipping.");
				//continue;
			}
			else
			normalsAccessor = attributeIt->second;


			// texcoords accessor
			attributeIt = primitive.attributes.find("TEXCOORD_0");
			if (attributeIt != primitive.attributes.end())
			{
				texCoordsAccessor = attributeIt->second;
			}

			// weights accessor
			attributeIt = primitive.attributes.find("WEIGHTS_0");
			if (attributeIt != primitive.attributes.end())
			{
				weightsAccessor = attributeIt->second;
			}

			// joints accessor
			attributeIt = primitive.attributes.find("JOINTS_0");
			if (attributeIt != primitive.attributes.end())
			{
				jointsAccessor = attributeIt->second;
			}

			ImportedMesh rawMesh{};
			rawMesh.m_name = gltfMesh.name + std::to_string(i);
			rawMesh.m_materialIndex = primitive.material == -1 ? 0 : primitive.material;

			for (size_t j = 0; j < triangleCount; ++j)
			{
				for (size_t k = 0; k < 3; ++k)
				{
					bool res = true;
					size_t index = j * 3 + k;
					if (indexed)
					{
						int64_t idx = 0;
						res = getIntBufferData(gltfModel, primitive.indices, j * 3 + k, 1, &idx);
						assert(res);
						index = (size_t)idx;
					}

					// position
					glm::vec3 position = glm::vec3();
					res = getFloatBufferData(gltfModel, positionsAccessor, index, 3, &position[0]);
					assert(res);
					position = globalTransform * glm::vec4(position, 1.0f);

					// normal
					glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
					if (normalsAccessor != -1)
					{
						res = getFloatBufferData(gltfModel, normalsAccessor, index, 3, &normal[0]);
						assert(res);
						normal = glm::normalize(normal);// normalTransform *glm::vec4(glm::normalize(normal), 0.0f);
					}
					

					// texcoord
					glm::vec2 uv = glm::vec2();
					if (texCoordsAccessor != -1)
					{
						res = getFloatBufferData(gltfModel, texCoordsAccessor, index, 2, &uv[0]);
						assert(res);

						if (invertTexcoordY)
						{
							uv.y = 1.0f - uv.y;
						}
					}

					// weights
					glm::vec4 weights = glm::vec4();
					if (weightsAccessor)
					{
						res = getFloatBufferData(gltfModel, weightsAccessor, index, 4, &weights[0]);
						assert(res);
						weights /= weights.x + weights.y + weights.z + weights.w;
					}

					// joints
					glm::uvec4 joints = glm::uvec4();
					if (jointsAccessor)
					{
						int64_t w[4];
						res = getIntBufferData(gltfModel, jointsAccessor, index, 4, w);
						assert(res);
						joints = glm::make_vec4(w);
						joints.x = jointMap[joints.x];
						joints.y = jointMap[joints.y];
						joints.z = jointMap[joints.z];
						joints.w = jointMap[joints.w];
					}

					rawMesh.m_aabbMin = glm::min(rawMesh.m_aabbMin, position);
					rawMesh.m_aabbMax = glm::max(rawMesh.m_aabbMax, position);

					rawMesh.m_positions.push_back(position);
					rawMesh.m_normals.push_back(normal);
					rawMesh.m_texCoords.push_back(uv);

					if (jointsAccessor && weightsAccessor)
					{
						rawMesh.m_weights.push_back(weights);
						rawMesh.m_joints.push_back(joints);
					}
				}
			}

			// allocate space for tangents
			rawMesh.m_tangents.resize(rawMesh.m_normals.size());

			resultModel.m_aabbMin = glm::min(rawMesh.m_aabbMin, rawMesh.m_aabbMin);
			resultModel.m_aabbMax = glm::max(rawMesh.m_aabbMax, rawMesh.m_aabbMax);
			resultModel.m_meshes.push_back(rawMesh);
		}
	}

	for (auto childNodeIdx : node.children)
	{
		loadNodes(childNodeIdx, globalTransform, gltfModel, jointMap, invertTexcoordY, resultModel);
	}
}

int recursiveSearch(const tinygltf::Model &gltfModel, int nodeToSearch, int currentNode, int &currentJointCount)
{
	if (std::find(gltfModel.skins[0].joints.cbegin(), gltfModel.skins[0].joints.cend(), currentNode) != gltfModel.skins[0].joints.end())
	{
		if (currentNode == nodeToSearch)
		{
			return currentJointCount;
		}
		++currentJointCount;
	}

	for (auto c : gltfModel.nodes[currentNode].children)
	{
		int r = recursiveSearch(gltfModel, nodeToSearch, c, currentJointCount);
		if (r != -1)
		{
			return r;
		}
	}
	return -1;
}

void listBoneNames(const tinygltf::Model &gltfModel, int currentNode = 0)
{
	if (std::find(gltfModel.skins[0].joints.cbegin(), gltfModel.skins[0].joints.cend(), currentNode) != gltfModel.skins[0].joints.end())
	{
		Log::info(gltfModel.nodes[currentNode].name.c_str());
	}

	for (auto c : gltfModel.nodes[currentNode].children)
	{
		listBoneNames(gltfModel, c);
	}
}

static int findSkeletonRootNodeIndex(const tinygltf::Model &model)
{
	std::vector<bool> skeletonNodes(model.nodes.size());
	std::vector<bool> hasParent(model.nodes.size());

	for (auto j : model.skins[0].joints)
	{
		skeletonNodes[j] = true;
	}

	for (size_t i = 0; i < model.nodes.size(); ++i)
	{
		if (skeletonNodes[i])
		{
			for (auto c : model.nodes[i].children)
			{
				hasParent[c] = true;
			}
		}
	}

	for (size_t i = 0; i < model.nodes.size(); ++i)
	{
		if (skeletonNodes[i] && !hasParent[i])
		{
			return (int)i;
		}
	}
	return -1;
}

bool GLTFLoader::loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, ImportedModel &model)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
	//bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

	if (!warn.empty())
	{
		Log::warn(warn.c_str());
	}

	if (!err.empty())
	{
		Log::err(err.c_str());
	}

	if (!ret)
	{
		Log::err("Failed to parse glTF!");
		return false;
	}

	// import materials
	{
		auto getTexturePath = [](const tinygltf::Model &model, int textureIndex) -> std::string
		{
			if (textureIndex >= 0 && textureIndex < model.textures.size())
			{
				return model.images[model.textures[textureIndex].source].uri;
			}
			else
			{
				return "";
			}
		};

		// check if we need to create a default material
		if (gltfModel.materials.empty())
		{
			ImportedMaterial mat{};
			mat.m_name = "null";
			mat.m_alpha = ImportedMaterial::Alpha::OPAQUE;
			mat.m_albedoFactor = glm::vec3(1.0f);
			mat.m_metalnessFactor = 0.0f;
			mat.m_roughnessFactor = 1.0f;
			mat.m_emissiveFactor = glm::vec3(0.0f);
			mat.m_opacity = 1.0f;
			mat.m_albedoTexture = "";
			mat.m_normalTexture = "";
			mat.m_metalnessTexture = "";
			mat.m_roughnessTexture = "";
			mat.m_occlusionTexture = "";
			mat.m_emissiveTexture = "";
			mat.m_displacementTexture = "";

			model.m_materials.push_back(mat);

			Log::warn("No materials found. Using default material.");
		}

		for (size_t i = 0; i < gltfModel.materials.size(); ++i)
		{
			const auto &gltfMat = gltfModel.materials[i];

			ImportedMaterial mat{};
			mat.m_name = gltfMat.name;
			mat.m_alpha = gltfMat.alphaMode == "OPAQUE" ? ImportedMaterial::Alpha::OPAQUE : gltfMat.alphaMode == "MASK" ? ImportedMaterial::Alpha::MASKED : ImportedMaterial::Alpha::BLENDED;
			mat.m_albedoFactor = (glm::vec3)glm::make_vec3(gltfMat.pbrMetallicRoughness.baseColorFactor.data());
			mat.m_metalnessFactor = (float)gltfMat.pbrMetallicRoughness.metallicFactor;
			mat.m_roughnessFactor = (float)gltfMat.pbrMetallicRoughness.roughnessFactor;
			mat.m_emissiveFactor = (glm::vec3)glm::make_vec3(gltfMat.emissiveFactor.data());
			mat.m_opacity = (float)gltfMat.pbrMetallicRoughness.baseColorFactor[3];
			mat.m_albedoTexture = getTexturePath(gltfModel, gltfMat.pbrMetallicRoughness.baseColorTexture.index);
			mat.m_normalTexture = getTexturePath(gltfModel, gltfMat.normalTexture.index);
			mat.m_metalnessTexture = getTexturePath(gltfModel, gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
			mat.m_roughnessTexture = getTexturePath(gltfModel, gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
			mat.m_occlusionTexture = getTexturePath(gltfModel, gltfMat.occlusionTexture.index);
			mat.m_emissiveTexture = getTexturePath(gltfModel, gltfMat.emissiveTexture.index);
			mat.m_displacementTexture = "";

			model.m_materials.push_back(mat);
		}
	}
	
	if (!gltfModel.skins.empty())
	{
		std::vector<int> jointMap = gltfModel.skins[0].joints;

		int skeletonRoot = findSkeletonRootNodeIndex(gltfModel);

		for (auto &j : jointMap)
		{
			int jointCount = 0;
			j = recursiveSearch(gltfModel, j, skeletonRoot, jointCount);
		}

		listBoneNames(gltfModel);

		// load meshes
		loadNodes(0, glm::mat4(), gltfModel, jointMap, invertTexcoordY, model);
	}

	return true;
}