#include "ModelImporter.h"
#include "mikktspace.h"
#include "ImportedModel.h"
#include "meshoptimizer/meshoptimizer.h"
#include "WavefrontOBJLoader.h"
#include "GLTFLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <unordered_set>
#include <glm/mat3x3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <Log.h>
#include <physics/Physics.h>

namespace
{
	typedef bool (*LoaderFuncPtr)(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes, bool importSkeletons, bool importAnimations, ImportedModel &model);

	template<typename T>
	struct IndexedMesh
	{
		std::vector<T> indices;
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec4> tangents;
		std::vector<glm::vec2> texCoords;
		std::vector<glm::vec4> weights;
		std::vector<glm::uvec4> joints;
	};

	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec4 tangent;
		glm::vec2 texCoord;
		glm::vec4 weights;
		glm::uvec4 joints;
	};

	inline bool operator==(const Vertex &lhs, const Vertex &rhs)
	{
		return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
	}

	template <class T>
	inline void hashCombine(size_t &s, const T &v)
	{
		std::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}

	struct VertexHash
	{
		inline size_t operator()(const Vertex &value) const
		{
			size_t hashValue = 0;

			for (size_t i = 0; i < sizeof(Vertex); ++i)
			{
				hashCombine(hashValue, reinterpret_cast<const char *>(&value)[i]);
			}

			return hashValue;
		}
	};
}

// Returns the number of faces (triangles/quads) on the mesh to be processed.
static int mikktGetNumFaces(const SMikkTSpaceContext *pContext)
{
	ImportedMesh *mesh = reinterpret_cast<ImportedMesh *>(pContext->m_pUserData);
	return (int)(mesh->m_positions.size() / 3);
}

// Returns the number of vertices on face number iFace
// iFace is a number in the range {0, 1, ..., getNumFaces()-1}
static int mikktGetNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace)
{
	return 3;
}

// returns the position/normal/texcoord of the referenced face of vertex number iVert.
// iVert is in the range {0,1,2} for triangles and {0,1,2,3} for quads.
static void mikktGetPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert)
{
	ImportedMesh *mesh = reinterpret_cast<ImportedMesh *>(pContext->m_pUserData);
	const auto &pos = mesh->m_positions[iFace * 3 + iVert];
	fvPosOut[0] = pos.x;
	fvPosOut[1] = pos.y;
	fvPosOut[2] = pos.z;
}

static void mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert)
{
	ImportedMesh *mesh = reinterpret_cast<ImportedMesh *>(pContext->m_pUserData);
	const auto &norm = mesh->m_normals[iFace * 3 + iVert];
	fvNormOut[0] = norm.x;
	fvNormOut[1] = norm.y;
	fvNormOut[2] = norm.z;
}

static void mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert)
{
	ImportedMesh *mesh = reinterpret_cast<ImportedMesh *>(pContext->m_pUserData);
	const auto &tc = mesh->m_texCoords[iFace * 3 + iVert];
	fvTexcOut[0] = tc.x;
	fvTexcOut[1] = tc.y;
}

// either (or both) of the two setTSpace callbacks can be set.
// The call-back m_setTSpaceBasic() is sufficient for basic normal mapping.

// This function is used to return the tangent and fSign to the application.
// fvTangent is a unit length vector.
// For normal maps it is sufficient to use the following simplified version of the bitangent which is generated at pixel/vertex level.
// bitangent = fSign * cross(vN, tangent);
// Note that the results are returned unindexed. It is possible to generate a new index list
// But averaging/overwriting tangent spaces by using an already existing index list WILL produce INCRORRECT results.
// DO NOT! use an already existing index list.
static void mikktSetTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
{
	ImportedMesh *mesh = reinterpret_cast<ImportedMesh *>(pContext->m_pUserData);
	mesh->m_tangents[iFace * 3 + iVert] = glm::vec4(fvTangent[0], fvTangent[1], fvTangent[2], fSign);
}

static void createMaterialLibrary(const std::vector<ImportedMaterial> &materials, const std::string dstFilePath)
{
	nlohmann::json j;

	j["count"] = materials.size();
	j["materials"] = nlohmann::json::array();

	for (const auto &mat : materials)
	{
		j["materials"].push_back(
			{
				{ "name", mat.m_name },
				{ "alphaMode", (int)mat.m_alpha },
				{ "albedo", { mat.m_albedoFactor[0], mat.m_albedoFactor[1], mat.m_albedoFactor[2] } },
				{ "metalness", mat.m_metalnessFactor },
				{ "roughness", mat.m_roughnessFactor },
				{ "emissive", { mat.m_emissiveFactor[0], mat.m_emissiveFactor[1], mat.m_emissiveFactor[2] } },
				{ "opacity", mat.m_opacity },
				{ "albedoTexture", mat.m_albedoTexture },
				{ "normalTexture", mat.m_normalTexture },
				{ "metalnessTexture", mat.m_metalnessTexture },
				{ "roughnessTexture", mat.m_roughnessTexture },
				{ "occlusionTexture", mat.m_occlusionTexture },
				{ "emissiveTexture", mat.m_emissiveTexture },
				{ "displacementTexture", mat.m_displacementTexture }
			}
		);
	}

	std::ofstream infoFile(dstFilePath, std::ios::out | std::ios::trunc);
	infoFile << std::setw(4) << j << std::endl;
	infoFile.close();
}

template<typename T>
static IndexedMesh<T> generateOptimizedMesh(size_t faceCount, const glm::vec3 *positions, const glm::vec3 *normals, const glm::vec4 *tangents, const glm::vec2 *texCoords, const glm::vec4 *weights, const glm::uvec4 *joints, bool optimizeVertexOrder)
{
	meshopt_Stream streams[] =
	{
		{positions, sizeof(glm::vec3), sizeof(glm::vec3)},
		{normals, sizeof(glm::vec3), sizeof(glm::vec3)},
		{tangents, sizeof(glm::vec4), sizeof(glm::vec4)},
		{texCoords, sizeof(glm::vec2), sizeof(glm::vec2)},
		{weights, sizeof(glm::vec4), sizeof(glm::vec4)},
		{joints, sizeof(glm::uvec4), sizeof(glm::uvec4)},
	};

	// generate indices
	std::vector<unsigned int> remap(faceCount * 3);
	size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.data(), nullptr, faceCount * 3, faceCount * 3, streams, weights == nullptr ? 4 : 6);

	// fill new index and vertex buffers
	std::vector<T> indices(faceCount * 3);
	std::vector<glm::vec3> positionsIndexed(uniqueVertexCount);
	std::vector<glm::vec3> normalsIndexed(uniqueVertexCount);
	std::vector<glm::vec4> tangentsIndexed(uniqueVertexCount);
	std::vector<glm::vec2> texCoordsIndexed(uniqueVertexCount);
	std::vector<glm::vec4> weightsIndexed;
	std::vector<glm::uvec4> jointsIndexed;
	if (weights)
	{
		weightsIndexed.resize(uniqueVertexCount);
		jointsIndexed.resize(uniqueVertexCount);
	}

	meshopt_remapIndexBuffer(indices.data(), (T *)nullptr, indices.size(), remap.data());
	meshopt_remapVertexBuffer(positionsIndexed.data(), positions, faceCount * 3, sizeof(glm::vec3), remap.data());
	meshopt_remapVertexBuffer(normalsIndexed.data(), normals, faceCount * 3, sizeof(glm::vec3), remap.data());
	meshopt_remapVertexBuffer(tangentsIndexed.data(), tangents, faceCount * 3, sizeof(glm::vec4), remap.data());
	meshopt_remapVertexBuffer(texCoordsIndexed.data(), texCoords, faceCount * 3, sizeof(glm::vec2), remap.data());
	if (weights)
	{
		meshopt_remapVertexBuffer(weightsIndexed.data(), weights, faceCount * 3, sizeof(glm::vec4), remap.data());
		meshopt_remapVertexBuffer(jointsIndexed.data(), joints, faceCount * 3, sizeof(glm::uvec4), remap.data());
	}

	// optimize vertex cache
	meshopt_optimizeVertexCache(indices.data(), indices.data(), faceCount * 3, uniqueVertexCount);


	// optimize vertex order
	if (optimizeVertexOrder)
	{
		meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), indices.size(), uniqueVertexCount);
		meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
		meshopt_remapVertexBuffer(positionsIndexed.data(), positionsIndexed.data(), positionsIndexed.size(), sizeof(glm::vec3), remap.data());
		meshopt_remapVertexBuffer(normalsIndexed.data(), normalsIndexed.data(), normalsIndexed.size(), sizeof(glm::vec3), remap.data());
		meshopt_remapVertexBuffer(tangentsIndexed.data(), tangentsIndexed.data(), tangentsIndexed.size(), sizeof(glm::vec4), remap.data());
		meshopt_remapVertexBuffer(texCoordsIndexed.data(), texCoordsIndexed.data(), texCoordsIndexed.size(), sizeof(glm::vec2), remap.data());
		if (weights)
		{
			meshopt_remapVertexBuffer(weightsIndexed.data(), weightsIndexed.data(), weightsIndexed.size(), sizeof(glm::vec4), remap.data());
			meshopt_remapVertexBuffer(jointsIndexed.data(), jointsIndexed.data(), jointsIndexed.size(), sizeof(glm::uvec4), remap.data());
		}
	}

	return { indices, positionsIndexed, normalsIndexed, tangentsIndexed, texCoordsIndexed, weightsIndexed, jointsIndexed };
}

void storeSkeletonsAndAnimations(const ImportedModel &model, const std::string &baseDstPath)
{
	// store skeletons
	for (size_t i = 0; i < model.m_skeletons.size(); ++i)
	{
		std::string dstPath = model.m_skeletons.size() > 1 ? (baseDstPath + "_skeleton" + std::to_string(i) + ".skel") : (baseDstPath + ".skel");
		std::ofstream dstFile(dstPath, std::ios::out | std::ios::binary | std::ios::trunc);

		const auto &skele = model.m_skeletons[i];

		// write joint count
		const uint32_t jointCount = static_cast<uint32_t>(skele.m_joints.size());
		dstFile.write((const char *)&jointCount, 4);

		// write joints
		for (size_t j = 0; j < jointCount; ++j)
		{
			dstFile.write((const char *)&skele.m_joints[j].m_invBindPose[0][0], sizeof(float) * 16);
		}

		// write parent indices
		for (size_t j = 0; j < jointCount; ++j)
		{
			dstFile.write((const char *)&skele.m_joints[j].m_parentIdx, sizeof(ImportedJoint::m_parentIdx));
		}

		// write joint names
		for (size_t j = 0; j < jointCount; ++j)
		{
			const char *str = skele.m_joints[j].m_name.c_str();
			dstFile.write(str, strlen(str) + 1);
		}
	}

	// store animations
	for (size_t i = 0; i < model.m_animationClips.size(); ++i)
	{
		const auto &animClip = model.m_animationClips[i];

		std::string cleanedName = animClip.m_name;
		cleanedName.erase(std::remove_if(cleanedName.begin(), cleanedName.end(), [](auto c)
			{
				switch (c)
				{
				case '\\':
				case '/':
				case ':':
				case '*':
				case '?':
				case '"':
				case '<':
				case '>':
				case '|':
					return true;
				default:
					return false;
				}
			}), cleanedName.end());

		std::string dstPath = model.m_animationClips.size() > 1 ? (baseDstPath + "_" + cleanedName + ".anim") : baseDstPath + ".anim";
		std::ofstream dstFile(dstPath, std::ios::out | std::ios::binary | std::ios::trunc);

		// write joint count
		const uint32_t jointCount = (uint32_t)animClip.m_jointAnimations.size();
		dstFile.write((const char *)&jointCount, 4);

		// write duration
		dstFile.write((const char *)&animClip.m_duration, 4);

		uint32_t curTranslationArrayOffset = 0;
		uint32_t curRotationArrayOffset = 0;
		uint32_t curScaleArrayOffset = 0;

		// write joint clip info
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			
			uint32_t translationCount = (uint32_t)jointClip.m_translationChannel.m_timeKeys.size();
			uint32_t rotationCount = (uint32_t)jointClip.m_rotationChannel.m_timeKeys.size();
			uint32_t scaleCount = (uint32_t)jointClip.m_scaleChannel.m_timeKeys.size();

			dstFile.write((const char *)&translationCount, 4);
			dstFile.write((const char *)&rotationCount, 4);
			dstFile.write((const char *)&scaleCount, 4);
			dstFile.write((const char *)&curTranslationArrayOffset, 4);
			dstFile.write((const char *)&curRotationArrayOffset, 4);
			dstFile.write((const char *)&curScaleArrayOffset, 4);

			curTranslationArrayOffset += (uint32_t)jointClip.m_translationChannel.m_timeKeys.size();
			curRotationArrayOffset += (uint32_t)jointClip.m_rotationChannel.m_timeKeys.size();
			curScaleArrayOffset += (uint32_t)jointClip.m_scaleChannel.m_timeKeys.size();
		}

		// write translation timekeys
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			for (auto t : jointClip.m_translationChannel.m_timeKeys)
			{
				dstFile.write((const char *)&t, 4);
			}
		}

		// write rotation timekeys
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			for (auto t : jointClip.m_rotationChannel.m_timeKeys)
			{
				dstFile.write((const char *)&t, 4);
			}
		}

		// write scale timekeys
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			for (auto t : jointClip.m_scaleChannel.m_timeKeys)
			{
				dstFile.write((const char *)&t, 4);
			}
		}

		// write translations
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			for (const auto &t : jointClip.m_translationChannel.m_translations)
			{
				dstFile.write((const char *)&t.x, sizeof(float) * 3);
			}
		}

		// write rotations
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			for (const auto &r : jointClip.m_rotationChannel.m_rotations)
			{
				dstFile.write((const char *)&r.x, sizeof(float) * 4);
			}
		}

		// write scales
		for (const auto &jointClip : animClip.m_jointAnimations)
		{
			for (const auto &s : jointClip.m_scaleChannel.m_scales)
			{
				dstFile.write((const char *)&s, sizeof(float));
			}
		}
	}
}

bool ModelImporter::importModel(const ImportOptions &importOptions, Physics *physics, const char *srcPath, const char *dstPath)
{
	SMikkTSpaceInterface mikkTSpaceInterface = {};
	mikkTSpaceInterface.m_getNumFaces = mikktGetNumFaces;
	mikkTSpaceInterface.m_getNumVerticesOfFace = mikktGetNumVerticesOfFace;
	mikkTSpaceInterface.m_getPosition = mikktGetPosition;
	mikkTSpaceInterface.m_getNormal = mikktGetNormal;
	mikkTSpaceInterface.m_getTexCoord = mikktGetTexCoord;
	mikkTSpaceInterface.m_setTSpaceBasic = mikktSetTSpaceBasic;
	mikkTSpaceInterface.m_setTSpace = nullptr;

	// select correct loader function pointer
	LoaderFuncPtr loader = WavefrontOBJLoader::loadModel;
	switch (importOptions.m_fileType)
	{
	case FileType::WAVEFRONT_OBJ:
		loader = WavefrontOBJLoader::loadModel;
		break;
	case FileType::GLTF:
		loader = GLTFLoader::loadModel;
		break;
	default:
		assert(false);
		break;
	}

	// load model
	ImportedModel model;
	if (!loader(srcPath, importOptions.m_mergeByMaterial, importOptions.m_invertTexCoordY, importOptions.m_importMeshes, importOptions.m_importSkeletons, importOptions.m_importAnimations, model))
	{
		Log::err("Import failed!");
		return false;
	}

	std::string dstFileName = dstPath;
	
	// write skeleton and animations
	storeSkeletonsAndAnimations(model, dstFileName);
	
	if (!model.m_meshes.empty())
	{
		nlohmann::json j;

		// create material library
		createMaterialLibrary(model.m_materials, dstFileName + ".matlib");

		j["meshFile"] = dstFileName + ".mesh";
		j["materialLibraryFile"] = dstFileName + ".matlib";
		j["subMeshes"] = nlohmann::json::array();
		j["subMeshInstances"] = nlohmann::json::array();

		std::ofstream dstFile(dstFileName + ".mesh", std::ios::out | std::ios::binary | std::ios::trunc);

		size_t fileOffset = 0;
		size_t subMeshIndex = 0;

		// cook physics meshes
		{
			// generate indexed physics mesh
			eastl::vector<float> indexedPositions;
			eastl::vector<uint32_t> indices;
			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;
			{
				eastl::vector<float> positions;
				for (auto &mesh : model.m_meshes)
				{
					for (auto &p : mesh.m_positions)
					{
						positions.push_back(p.x);
						positions.push_back(p.y);
						positions.push_back(p.z);
					}
				}

				meshopt_Stream streams[] =
				{
					{positions.data(), sizeof(glm::vec3), sizeof(glm::vec3)},
				};

				indexCount = (uint32_t)positions.size() / 3;

				// generate indices
				eastl::vector<unsigned int> remap(indexCount);
				vertexCount = (uint32_t)meshopt_generateVertexRemapMulti(remap.data(), nullptr, indexCount, indexCount, streams, 1);

				// fill new index and vertex buffers
				indices.resize(indexCount);
				indexedPositions.resize(vertexCount * 3);

				meshopt_remapIndexBuffer(indices.data(), (uint32_t *)nullptr, indices.size(), remap.data());
				meshopt_remapVertexBuffer(indexedPositions.data(), positions.data(), indexCount, sizeof(glm::vec3), remap.data());
			}

			// cook
			char *physicsConvexMeshData = nullptr;
			uint32_t physicsConvexMeshSize = 0;
			char *physicsTriangleMeshData = nullptr;
			uint32_t physicsTriangleMeshSize = 0;

			bool res = physics->cookConvexMesh(vertexCount, indexedPositions.data(), &physicsConvexMeshSize, &physicsConvexMeshData);
			assert(res);
			res = physics->cookTriangleMesh(indexCount, indices.data(), vertexCount, indexedPositions.data(), &physicsTriangleMeshSize, &physicsTriangleMeshData);
			assert(res);

			// write to file
			j["physicsConvexMeshDataOffset"] = fileOffset;
			j["physicsConvexMeshDataSize"] = physicsConvexMeshSize;
			dstFile.write(physicsConvexMeshData, physicsConvexMeshSize);
			fileOffset += physicsConvexMeshSize;

			j["physicsTriangleMeshDataOffset"] = fileOffset;
			j["physicsTriangleMeshDataSize"] = physicsTriangleMeshSize;
			dstFile.write(physicsTriangleMeshData, physicsTriangleMeshSize);
			fileOffset += physicsTriangleMeshSize;

			// free memory
			delete[] physicsConvexMeshData;
			delete[] physicsTriangleMeshData;
		}

		uint64_t totalFaceCount = 0;
		for (auto &mesh : model.m_meshes)
		{
			totalFaceCount += mesh.m_positions.size() / 3;
		}

		uint64_t actualFaceCount = 0;
		uint32_t matrixPaletteSize = 0;

		for (auto &mesh : model.m_meshes)
		{
			// generate tangents
			{
				SMikkTSpaceContext mikkTSpaceContext = {};
				mikkTSpaceContext.m_pInterface = &mikkTSpaceInterface;
				mikkTSpaceContext.m_pUserData = &mesh;

				auto result = genTangSpaceDefault(&mikkTSpaceContext);
				assert(result != 0);
			}

			// generate optimized 32bit indexed mesh
			auto indexedMesh32 = generateOptimizedMesh<uint32_t>(mesh.m_positions.size() / 3, mesh.m_positions.data(), mesh.m_normals.data(), mesh.m_tangents.data(), mesh.m_texCoords.data(),
				mesh.m_weights.data(), mesh.m_joints.data(), false);

			// find size of matrix palette by looking for largest joint index and adding 1
			if (!mesh.m_joints.empty())
			{
				for (const auto &wi : indexedMesh32.joints)
				{
					matrixPaletteSize = std::max(matrixPaletteSize, wi[0] + 1);
					matrixPaletteSize = std::max(matrixPaletteSize, wi[1] + 1);
					matrixPaletteSize = std::max(matrixPaletteSize, wi[2] + 1);
					matrixPaletteSize = std::max(matrixPaletteSize, wi[3] + 1);
				}
			}

			// generate smaller 64k meshes
			{
				glm::vec3 aabbMin = glm::vec3(std::numeric_limits<float>::max());
				glm::vec3 aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
				glm::vec2 uvAabbMin = glm::vec2(std::numeric_limits<float>::max());
				glm::vec2 uvAabbMax = glm::vec2(std::numeric_limits<float>::lowest());

				std::vector<glm::vec3> positions;
				std::vector<glm::vec3> normals;
				std::vector<glm::vec4> tangents;
				std::vector<glm::vec2> texCoords;
				std::vector<glm::vec4> weights;
				std::vector<glm::uvec4> joints;

				// keep track of the number of unique vertices -> we are targeting 16bit indices, so we need to stay below 2^16 - 1 vertices
				std::unordered_set<Vertex, VertexHash> vertexSet;

				size_t processedIndexCount = 0;
				for (size_t i = 0; i < indexedMesh32.indices.size(); i += 3)
				{
					// process face
					for (size_t j = 0; j < 3; ++j)
					{
						uint32_t index = indexedMesh32.indices[i + j];

						Vertex v;
						v.position = indexedMesh32.positions[index];
						v.normal = indexedMesh32.normals[index];
						v.tangent = indexedMesh32.tangents[index];
						v.texCoord = indexedMesh32.texCoords[index];
						v.weights = indexedMesh32.weights.empty() ? glm::vec4() : indexedMesh32.weights[index];
						v.joints = indexedMesh32.joints.empty() ? glm::uvec4() : indexedMesh32.joints[index];

						vertexSet.insert(v);

						positions.push_back(v.position);
						normals.push_back(v.normal);
						tangents.push_back(v.tangent);
						texCoords.push_back(v.texCoord);
						if (!indexedMesh32.weights.empty())
						{
							weights.push_back(v.weights);
							joints.push_back(v.joints);
						}

						aabbMin = glm::min(aabbMin, v.position);
						aabbMax = glm::max(aabbMax, v.position);

						uvAabbMin = glm::min(uvAabbMin, v.texCoord);
						uvAabbMax = glm::max(uvAabbMax, v.texCoord);
					}

					const size_t localIndexOffset = i - processedIndexCount;

					// we reached 64k faces or 64k unique vertices or we processed the whole mesh
					if ((localIndexOffset / 3 + 1) > UINT16_MAX || (vertexSet.size() + 3) > UINT16_MAX || (i + 3) == indexedMesh32.indices.size())
					{
						auto indexedMesh16 = generateOptimizedMesh<uint16_t>(positions.size() / 3, positions.data(), normals.data(), tangents.data(), texCoords.data(), weights.data(), joints.data(), true);

						// quantize data
						std::vector<uint16_t> quantizedPositions;
						std::vector<uint16_t> quantizedQTangents;
						std::vector<uint16_t> quantizedTexCoords;
						std::vector<uint32_t> quantizedWeights;
						std::vector<uint32_t> quantizedJoints;
						quantizedPositions.reserve(3 * indexedMesh16.positions.size());
						quantizedQTangents.reserve(4 * indexedMesh16.positions.size());
						quantizedTexCoords.reserve(2 * indexedMesh16.positions.size());
						if (!weights.empty())
						{
							quantizedWeights.reserve(1 * indexedMesh16.positions.size());
							quantizedJoints.reserve(2 * indexedMesh16.positions.size());
						}

						glm::vec3 scale = 1.0f / (aabbMax - aabbMin);
						glm::vec3 bias = -aabbMin * scale;

						glm::vec2 texCoordScale = 1.0f / (uvAabbMax - uvAabbMin);
						glm::vec2 texCoordBias = -uvAabbMin * texCoordScale;

						for (size_t j = 0; j < indexedMesh16.positions.size(); ++j)
						{
							// position
							{
								glm::vec3 normalizedPosition = indexedMesh16.positions[j] * scale + bias;
								quantizedPositions.push_back(meshopt_quantizeUnorm(normalizedPosition.x, 16));
								quantizedPositions.push_back(meshopt_quantizeUnorm(normalizedPosition.y, 16));
								quantizedPositions.push_back(meshopt_quantizeUnorm(normalizedPosition.z, 16));
							}

							// texcoord
							{
								glm::vec2 normalizedTexCoord = indexedMesh16.texCoords[j] * texCoordScale + texCoordBias;
								quantizedTexCoords.push_back(meshopt_quantizeUnorm(normalizedTexCoord.x, 16));
								quantizedTexCoords.push_back(meshopt_quantizeUnorm(normalizedTexCoord.y, 16));
							}

							// qtangent
							{
								glm::vec4 tangent = indexedMesh16.tangents[j];
								glm::vec3 normal = indexedMesh16.normals[j];
								glm::vec3 bitangent = glm::cross(normal, glm::vec3(tangent));

								glm::mat3 tbn = glm::mat3(glm::normalize(glm::vec3(tangent)),
									glm::normalize(bitangent),
									glm::normalize(normal));
								glm::quat q = glm::quat_cast(tbn);

								// ensure q.w is always positive
								if (q.w < 0.0f)
								{
									q = -q;
								}

								// make sure q.w never becomes 0.0
								constexpr float bias = 1.0f / INT16_MAX;
								if (q.w < bias)
								{
									float normFactor = sqrtf(1.0f - bias * bias);
									q.x *= normFactor;
									q.y *= normFactor;
									q.z *= normFactor;
									q.w = bias;
								}

								// encode sign of bitangent in quaternion (q == -q)
								if (tangent.w < 0.0f)
								{
									q = -q;
								}

								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.x * 0.5f + 0.5f, 16));
								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.y * 0.5f + 0.5f, 16));
								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.z * 0.5f + 0.5f, 16));
								quantizedQTangents.push_back(meshopt_quantizeUnorm(q.w * 0.5f + 0.5f, 16));
							}

							if (!weights.empty())
							{
								auto packUnorm4x8 = [](const glm::vec4 &v)
								{
									uint32_t result = 0;
									result |= ((uint32_t)glm::round(glm::clamp(v.x, 0.0f, 1.0f) * 255.0f)) << 0;
									result |= ((uint32_t)glm::round(glm::clamp(v.y, 0.0f, 1.0f) * 255.0f)) << 8;
									result |= ((uint32_t)glm::round(glm::clamp(v.z, 0.0f, 1.0f) * 255.0f)) << 16;
									result |= ((uint32_t)glm::round(glm::clamp(v.w, 0.0f, 1.0f) * 255.0f)) << 24;

									return result;
								};

								quantizedWeights.push_back(packUnorm4x8(indexedMesh16.weights[j]));

								quantizedJoints.push_back((indexedMesh16.joints[j].x & 0xFFFF) | ((indexedMesh16.joints[j].y & 0xFFFF) << 16));
								quantizedJoints.push_back((indexedMesh16.joints[j].z & 0xFFFF) | ((indexedMesh16.joints[j].w & 0xFFFF) << 16));
							}
						}


						// write to file
						dstFile.write((const char *)indexedMesh16.indices.data(), indexedMesh16.indices.size() * sizeof(uint16_t));
						dstFile.write((const char *)indexedMesh16.positions.data(), indexedMesh16.positions.size() * sizeof(glm::vec3));
						dstFile.write((const char *)indexedMesh16.normals.data(), indexedMesh16.normals.size() * sizeof(glm::vec3));
						dstFile.write((const char *)indexedMesh16.tangents.data(), indexedMesh16.tangents.size() * sizeof(glm::vec4));
						dstFile.write((const char *)indexedMesh16.texCoords.data(), indexedMesh16.texCoords.size() * sizeof(glm::vec2));
						if (!weights.empty())
						{
							dstFile.write((const char *)quantizedJoints.data(), quantizedJoints.size() * sizeof(uint32_t));
							dstFile.write((const char *)quantizedWeights.data(), quantizedWeights.size() * sizeof(uint32_t));
						}

						j["subMeshes"].push_back(
							{
								{ "name", mesh.m_name },
								{ "dataOffset", fileOffset },
								{ "vertexCount", indexedMesh16.positions.size() },
								{ "indexCount", indexedMesh16.indices.size() },
								{ "minCorner", { aabbMin.x,  aabbMin.y,  aabbMin.z } },
								{ "maxCorner", { aabbMax.x,  aabbMax.y,  aabbMax.z } },
								{ "texCoordMin", { uvAabbMin.x,  uvAabbMin.y } },
								{ "texCoordMax", { uvAabbMax.x,  uvAabbMax.y } },
								{ "skinned", !weights.empty() }
							});

						j["subMeshInstances"].push_back(
							{
								{ "name", mesh.m_name },
								{ "subMesh", j["subMeshes"].size() - 1 },
								{ "material", mesh.m_materialIndex }
							});

						fileOffset += indexedMesh16.positions.size() * sizeof(glm::vec3);
						fileOffset += indexedMesh16.normals.size() * sizeof(glm::vec3);
						fileOffset += indexedMesh16.tangents.size() * sizeof(glm::vec4);
						fileOffset += indexedMesh16.texCoords.size() * sizeof(glm::vec2);
						fileOffset += indexedMesh16.indices.size() * sizeof(uint16_t);
						fileOffset += quantizedJoints.size() * sizeof(uint32_t);
						fileOffset += quantizedWeights.size() * sizeof(uint32_t);

						assert(indexedMesh16.indices.size() % 3 == 0);
						actualFaceCount += indexedMesh16.indices.size() / 3;

						aabbMin = glm::vec3(std::numeric_limits<float>::max());
						aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
						uvAabbMin = glm::vec2(std::numeric_limits<float>::max());
						uvAabbMax = glm::vec2(std::numeric_limits<float>::lowest());
						positions.clear();
						normals.clear();
						tangents.clear();
						texCoords.clear();
						vertexSet.clear();

						processedIndexCount = i;

						Log::info(("Processed SubMesh # " + std::to_string(subMeshIndex++)).c_str());
					}
				}
			}
		}

		j["minCorner"] = { model.m_aabbMin.x, model.m_aabbMin.y, model.m_aabbMin.z };
		j["maxCorner"] = { model.m_aabbMax.x, model.m_aabbMax.y, model.m_aabbMax.z };
		j["matrixPaletteSize"] = matrixPaletteSize;

		dstFile.close();

		std::ofstream infoFile(dstFileName + ".info", std::ios::out | std::ios::trunc);
		infoFile << std::setw(4) << j << std::endl;
		infoFile.close();

		assert(totalFaceCount == actualFaceCount);
	}

	Log::info("Import succeeded!");
	return true;
}
