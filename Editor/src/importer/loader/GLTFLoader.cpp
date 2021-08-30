#include "GLTFLoader.h"
#include <nlohmann/json.hpp>
#include <Log.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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

	struct PerNodeData
	{
		std::string m_name;
		std::vector<bool> m_isJoint; // one per skeleton
		std::vector<glm::mat4> m_invBindMatrices; // one per skeleton
		std::vector<LoadedAnimationClip::JointAnimationClip> m_animClips;
	};

	struct ImportContext
	{
		std::vector<PerNodeData> m_perNodeData;
		std::vector<std::vector<int>> m_jointMap; // one per skeleton
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

static glm::mat4 getLocalNodeTransform(const tinygltf::Model &gltfModel, int nodeIdx)
{
	glm::mat4 localTransform = glm::identity<glm::mat4>();
	if (!isJoint(gltfModel, (int)nodeIdx))
	{
		const auto &node = gltfModel.nodes[nodeIdx];
		if (!node.matrix.empty())
		{
			localTransform = (glm::mat4)glm::make_mat4(node.matrix.data());
		}
		else if (!node.scale.empty() || !node.rotation.empty() || !node.translation.empty())
		{
			glm::vec3 scale = !node.scale.empty() ? (glm::vec3)glm::make_vec3(node.scale.data()) : glm::vec3(1.0f);
			glm::quat rot = !node.rotation.empty() ? (glm::quat)glm::make_quat(node.rotation.data()) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			glm::vec3 trans = !node.translation.empty() ? (glm::vec3)glm::make_vec3(node.translation.data()) : glm::vec3();

			localTransform = glm::translate(trans) * glm::mat4_cast(rot) * glm::scale(scale);
		}
	}

	return localTransform;
}

static void loadNodes(size_t nodeIdx, const glm::mat4 &parentTransform, const tinygltf::Model &gltfModel, const std::vector<std::vector<int>> &jointMaps, std::vector<bool> &visitedNodes, bool invertTexcoordY, LoadedModel &resultModel)
{
	const auto &node = gltfModel.nodes[nodeIdx];

	glm::mat4 localTransform = getLocalNodeTransform(gltfModel, (int)nodeIdx);
	glm::mat4 globalTransform = localTransform * parentTransform;

	if (!visitedNodes[nodeIdx])
	{
		visitedNodes[nodeIdx] = true;


		glm::mat4 normalTransform = glm::transpose(glm::inverse(globalTransform));

		bool skipMesh = false;

		if (node.mesh != -1 && node.skin > 0)
		{
			Log::warn("GLTFLoader: File has more than one skin. Skipping mesh \"%s\" with skin index %i!", gltfModel.meshes[node.mesh].name.c_str(), node.skin);
			skipMesh = true;
		}

		if (node.mesh != -1 && !skipMesh)
		{
			const auto &gltfMesh = gltfModel.meshes[node.mesh];
			Log::info("GLTFLoader: Importing Mesh \"%s\".", gltfMesh.name.c_str());

			for (size_t i = 0; i < gltfMesh.primitives.size(); ++i)
			{
				const auto &primitive = gltfMesh.primitives[i];
				Log::info("GLTFLoader: Importing Primitive #%i", (int)i);

				if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
				{
					Log::warn("GLTFLoader: glTF primitive has unsupported primitive mode (%i)! Skipping.", primitive.mode);
					continue;
				}

				const bool indexed = primitive.indices != -1;
				const size_t vertexCount = indexed ? gltfModel.accessors[primitive.indices].count : gltfModel.accessors[primitive.attributes.at("POSITION")].count;

				if (vertexCount % 3 != 0)
				{
					Log::warn("GLTFLoader: glTF primitive has a vertex count that is not divisible by 3 (%i)! Skipping.", vertexCount);
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
					Log::warn("GLTFLoader: glTF primitive does not have position attribute! Skipping.");
					continue;
				}

				positionsAccessor = attributeIt->second;


				// normals accessor
				attributeIt = primitive.attributes.find("NORMAL");
				if (attributeIt == primitive.attributes.end())
				{
					Log::warn("GLTFLoader: glTF primitive does not have normal attribute! Skipping.");
					continue;
				}

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

				LoadedModel::Mesh rawMesh{};
				rawMesh.m_name = gltfMesh.name.c_str() + eastl::to_string(i);
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
						//position = globalTransform * glm::vec4(position, 1.0f);

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
							assert(joints.x < jointMaps[node.skin].size());
							assert(joints.y < jointMaps[node.skin].size());
							assert(joints.z < jointMaps[node.skin].size());
							assert(joints.w < jointMaps[node.skin].size());
							joints = glm::make_vec4(w);
							joints.x = jointMaps[node.skin][joints.x];
							joints.y = jointMaps[node.skin][joints.y];
							joints.z = jointMaps[node.skin][joints.z];
							joints.w = jointMaps[node.skin][joints.w];
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
	}

	for (auto childNodeIdx : node.children)
	{
		loadNodes(childNodeIdx, globalTransform, gltfModel, jointMaps, visitedNodes, invertTexcoordY, resultModel);
	}
}


static void listBoneNames(const tinygltf::Model &gltfModel, int currentNode = 0)
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

static int findSkeletonRootNodeIndexAndTransform(const tinygltf::Model &model, glm::mat4 *rootTransform)
{
	std::vector<int> parentIndices(model.nodes.size(), -1);

	for (int i = 0; i < model.nodes.size(); ++i)
	{
		for (int child : model.nodes[i].children)
		{
			parentIndices[child] = i;
		}
	}

	int rootIndex = -1;
	
	if (model.skins[0].skeleton != -1)
	{
		rootIndex = model.skins[0].skeleton;
	}
	else
	{
		std::vector<bool> skeletonNodes(model.nodes.size());
		std::vector<bool> hasParent(model.nodes.size());

		// tag all nodes that are skeleton joints/nodes
		for (auto j : model.skins[0].joints)
		{
			skeletonNodes[j] = true;
		}

		// tag all skeleton nodes that have a parent
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

		// find first node without a parent. this is our root node
		for (size_t i = 0; i < model.nodes.size(); ++i)
		{
			if (skeletonNodes[i] && !hasParent[i])
			{
				rootIndex = (int)i;
				break;
			}
		}
	}
	
	if (rootIndex != -1)
	{
		glm::mat4 curTrans = glm::identity<glm::mat4>();
		int curIdx = rootIndex;

		while (parentIndices[curIdx] != -1)
		{
			curIdx = parentIndices[curIdx];
			glm::mat4 localTransform = getLocalNodeTransform(model, curIdx);
			curTrans = curTrans * localTransform;
		}

		*rootTransform = curTrans;
	}

	return rootIndex;
}

static void loadSkeletonNodesRecursive(
	const tinygltf::Model &gltfModel,
	int skeletonIndex,
	int nodeIdx,
	const glm::mat4 &parentTransform,
	uint32_t parentIdx,
	const std::vector<PerNodeData> &perNodeData,
	std::vector<int> &jointMap,
	std::vector<LoadedAnimationClip> &animClips,
	LoadedModel &resultModel)
{
	const tinygltf::Node &node = gltfModel.nodes[nodeIdx];

	glm::mat4 localTransform = getLocalNodeTransform(gltfModel, (int)nodeIdx);
	glm::mat4 globalTransform = parentTransform * localTransform;

	auto &skeleton = resultModel.m_skeletons[skeletonIndex];
	const auto gltfJointsArray = gltfModel.skins[skeletonIndex].joints;

	// is the current node part of the current skeleton?
	if (auto it = std::find(gltfJointsArray.cbegin(), gltfJointsArray.cend(), (int)nodeIdx); it != gltfJointsArray.cend())
	{
		LoadedSkeleton::Joint joint{};
		joint.m_name = node.name.c_str();
		joint.m_invBindPose = perNodeData[nodeIdx].m_invBindMatrices[skeletonIndex];// *glm::inverse(globalTransform);
		joint.m_parentIdx = parentIdx;

		skeleton.m_joints.push_back(joint);
		parentIdx = static_cast<uint32_t>(skeleton.m_joints.size() - 1);
		jointMap[it - gltfJointsArray.cbegin()] = static_cast<int>(parentIdx);

		// store the anim data of this joint for all anim clips
		for (size_t i = 0; i < gltfModel.animations.size(); ++i)
		{
			animClips[i].m_jointAnimations.push_back(perNodeData[nodeIdx].m_animClips[i]);
		}
	}

	for (int childNodeIdx : gltfModel.nodes[nodeIdx].children)
	{
		loadSkeletonNodesRecursive(gltfModel, skeletonIndex, childNodeIdx, globalTransform, parentIdx, perNodeData, jointMap, animClips, resultModel);
	}
}

static void loadSkeletonNodesAndAnimationClips(const tinygltf::Model &gltfModel, const std::vector<PerNodeData> &perNodeData, bool importSkeletons, bool importAnimations, std::vector<std::vector<int>> &jointMaps, LoadedModel &resultModel)
{
	jointMaps = {};
	jointMaps.resize(gltfModel.skins.size());
	resultModel.m_skeletons.resize(gltfModel.skins.size());

	for (size_t i = 0; i < gltfModel.skins.size(); ++i)
	{
		const auto &skin = gltfModel.skins[i];
		jointMaps[i] = skin.joints;
		glm::mat4 rootTransform = glm::identity<glm::mat4>();
		int skeletonRoot = findSkeletonRootNodeIndexAndTransform(gltfModel, &rootTransform);

		std::vector<LoadedAnimationClip> animClips(gltfModel.animations.size());

		// assign anim clip names
		for (size_t j = 0; j < gltfModel.animations.size(); ++j)
		{
			animClips[j].m_name = gltfModel.animations[j].name.empty() ? "animation_clip" + eastl::to_string(j) : gltfModel.animations[j].name.c_str();
			animClips[j].m_skeletonIndex = i;
		}

		loadSkeletonNodesRecursive(gltfModel, i, skeletonRoot, rootTransform, -1, perNodeData, jointMaps[i], animClips, resultModel);

		if (importAnimations)
		{
			// remove animation clips that dont affect this skeleton
			animClips.erase(std::remove_if(animClips.begin(), animClips.end(), [&](const auto &elem)
				{
					// iterate over all joints and check if they have any anim data
					for (auto &jointClip : elem.m_jointAnimations)
					{
						if (!jointClip.m_translationChannel.m_timeKeys.empty()
							|| !jointClip.m_rotationChannel.m_timeKeys.empty()
							|| !jointClip.m_scaleChannel.m_timeKeys.empty())
						{
							return false;
						}
					}

					return true;

				}), animClips.end());

			// compute durations and add to result model anim clips
			for (auto &clip : animClips)
			{
				for (auto &jointClip : clip.m_jointAnimations)
				{
					for (auto t : jointClip.m_translationChannel.m_timeKeys)
					{
						clip.m_duration = std::max(clip.m_duration, t);
					}

					for (auto t : jointClip.m_rotationChannel.m_timeKeys)
					{
						clip.m_duration = std::max(clip.m_duration, t);
					}

					for (auto t : jointClip.m_scaleChannel.m_timeKeys)
					{
						clip.m_duration = std::max(clip.m_duration, t);
					}
				}

				resultModel.m_animationClips.push_back(std::move(clip));
			}
		}

		if (!importSkeletons)
		{
			resultModel.m_skeletons.clear();
		}
	}
}

static std::vector<PerNodeData> generatePerNodeData(const tinygltf::Model &gltfModel)
{
	std::vector<PerNodeData> perNodeData(gltfModel.nodes.size());

	// reserve space
	for (auto &pnd : perNodeData)
	{
		pnd.m_isJoint.resize(gltfModel.skins.size());
		pnd.m_invBindMatrices.resize(gltfModel.skins.size());
		pnd.m_animClips.resize(gltfModel.animations.size());
	}

	// get bind matrices and mark joints
	for (size_t i = 0; i < gltfModel.skins.size(); ++i)
	{
		const auto &skin = gltfModel.skins[i];

		const tinygltf::Accessor &accessor = gltfModel.accessors[skin.inverseBindMatrices];
		const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

		const auto *invBindPoseArray = (const glm::mat4 *)&buffer.data[accessor.byteOffset + bufferView.byteOffset];

		for (size_t j = 0; j < skin.joints.size(); ++j)
		{
			int jointNode = skin.joints[j];
			perNodeData[jointNode].m_isJoint[i] = true;
			perNodeData[jointNode].m_invBindMatrices[i] = invBindPoseArray[j];
		}
	}

	// get animation clips
	for (size_t i = 0; i < gltfModel.animations.size(); ++i)
	{
		const auto &anim = gltfModel.animations[i];

		for (const auto &c : anim.channels)
		{
			enum class ChannelType
			{
				TRANSLATION, ROTATION, SCALE
			};

			ChannelType channelType = ChannelType::TRANSLATION;

			if (c.target_path == "translation")
			{
				channelType = ChannelType::TRANSLATION;
			}
			else if (c.target_path == "rotation")
			{
				channelType = ChannelType::ROTATION;
			}
			else if (c.target_path == "scale")
			{
				channelType = ChannelType::SCALE;
			}
			else
			{
				continue;
			}

			auto &jointClip = perNodeData[c.target_node].m_animClips[i];
			const auto &sampler = anim.samplers[c.sampler];

			// time keys
			{
				const tinygltf::Accessor &accessor = gltfModel.accessors[sampler.input];
				const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];


				auto &timeKeys = channelType == ChannelType::TRANSLATION ? jointClip.m_translationChannel.m_timeKeys : channelType == ChannelType::ROTATION ? jointClip.m_rotationChannel.m_timeKeys : jointClip.m_scaleChannel.m_timeKeys;

				timeKeys.resize(accessor.count);

				for (size_t j = 0; j < accessor.count; ++j)
				{
					timeKeys[j] = *(float *)&buffer.data[accessor.byteOffset + bufferView.byteOffset + j * sizeof(float)];
				}
			}

			const auto count = gltfModel.accessors[sampler.output].count;

			switch (channelType)
			{
			case ChannelType::TRANSLATION:
			{
				jointClip.m_translationChannel.m_translations.resize(count);

				for (size_t j = 0; j < count; ++j)
				{
					glm::vec3 translation = glm::vec3();
					bool res = getFloatBufferData(gltfModel, sampler.output, j, 3, &translation[0]);
					assert(res);
					jointClip.m_translationChannel.m_translations[j] = translation;
				}
				break;
			}
			case ChannelType::ROTATION:
			{
				jointClip.m_rotationChannel.m_rotations.resize(count);

				for (size_t j = 0; j < count; ++j)
				{
					float resArray[4] = {};
					bool res = getFloatBufferData(gltfModel, sampler.output, j, 4, resArray);
					assert(res);
					jointClip.m_rotationChannel.m_rotations[j][0] = resArray[0];
					jointClip.m_rotationChannel.m_rotations[j][1] = resArray[1];
					jointClip.m_rotationChannel.m_rotations[j][2] = resArray[2];
					jointClip.m_rotationChannel.m_rotations[j][3] = resArray[3];
				}
				break;
			}
			case ChannelType::SCALE:
			{
				jointClip.m_scaleChannel.m_scales.resize(count);

				for (size_t j = 0; j < count; ++j)
				{
					glm::vec3 scale = glm::vec3();
					bool res = getFloatBufferData(gltfModel, sampler.output, j, 3, &scale[0]);
					assert(res);
					jointClip.m_scaleChannel.m_scales[j] = scale.x;
				}
				break;
			}
			default:
				assert(false);
				break;
			}
		}
	}

	return perNodeData;
}

bool GLTFLoader::loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes, bool importSkeletons, bool importAnimations, LoadedModel &model)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
	//bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, argv[1]); // for binary glTF(.glb)

	if (!warn.empty())
	{
		Log::warn("GLTFLoader: Warning while loading file \"%s\": %s", filepath, warn.c_str());
	}

	if (!err.empty())
	{
		Log::err("GLTFLoader: Error while loading file \"%s\": %s", filepath, err.c_str());
	}

	if (!ret)
	{
		Log::err("GLTFLoader: Failed to load file \"%s\"!", filepath);
		return false;
	}

	// import materials (but only if there are meshes in this file)
	if (importMeshes && !gltfModel.meshes.empty())
	{
		auto getTexturePath = [](const tinygltf::Model &model, int textureIndex) -> eastl::string
		{
			if (textureIndex >= 0 && textureIndex < model.textures.size())
			{
				return model.images[model.textures[textureIndex].source].uri.c_str();
			}
			else
			{
				return "";
			}
		};

		// check if we need to create a default material
		if (gltfModel.materials.empty())
		{
			LoadedMaterial mat{};
			mat.m_name = "null";
			mat.m_alpha = LoadedMaterial::Alpha::OPAQUE;
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

			Log::warn("GLTFLoader: No materials found in file \"%s\". Using default material.", filepath);
		}

		for (size_t i = 0; i < gltfModel.materials.size(); ++i)
		{
			const auto &gltfMat = gltfModel.materials[i];

			LoadedMaterial mat{};
			mat.m_name = gltfMat.name.c_str();
			mat.m_alpha = gltfMat.alphaMode == "OPAQUE" ? LoadedMaterial::Alpha::OPAQUE : gltfMat.alphaMode == "MASK" ? LoadedMaterial::Alpha::MASKED : LoadedMaterial::Alpha::BLENDED;
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

	auto perNodeData = generatePerNodeData(gltfModel);
	std::vector<std::vector<int>> jointMaps;
	loadSkeletonNodesAndAnimationClips(gltfModel, perNodeData, importSkeletons, importAnimations, jointMaps, model);

	if (importMeshes)
	{
		std::vector<bool> visitedNodes(gltfModel.nodes.size());

		for (size_t i = 0; i < visitedNodes.size(); ++i)
		{
			if (!visitedNodes[i])
			{
				loadNodes(i, glm::identity<glm::mat4>(), gltfModel, jointMaps, visitedNodes, invertTexcoordY, model);
			}
		}
	}
	
	return true;
}
