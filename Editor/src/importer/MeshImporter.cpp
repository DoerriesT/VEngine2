#include "MeshImporter.h"
#include <asset/AssetManager.h>
#include <asset/MeshAsset.h>
#include <filesystem/VirtualFileSystem.h>
#include <Log.h>
#include "loader/LoadedModel.h"
#include "meshoptimizer/meshoptimizer.h"
#include "mikktspace.h"
#include <EASTL/string.h>
#include <EASTL/hash_set.h>
#include <physics/Physics.h>

namespace
{
	template<typename T>
	struct IndexedMesh
	{
		eastl::vector<T> indices;
		eastl::vector<glm::vec3> positions;
		eastl::vector<glm::vec3> normals;
		eastl::vector<glm::vec4> tangents;
		eastl::vector<glm::vec2> texCoords;
		eastl::vector<glm::vec4> weights;
		eastl::vector<glm::uvec4> joints;
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
	LoadedModel::Mesh *mesh = reinterpret_cast<LoadedModel::Mesh *>(pContext->m_pUserData);
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
	LoadedModel::Mesh *mesh = reinterpret_cast<LoadedModel::Mesh *>(pContext->m_pUserData);
	const auto &pos = mesh->m_positions[iFace * 3 + iVert];
	fvPosOut[0] = pos.x;
	fvPosOut[1] = pos.y;
	fvPosOut[2] = pos.z;
}

static void mikktGetNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert)
{
	LoadedModel::Mesh *mesh = reinterpret_cast<LoadedModel::Mesh *>(pContext->m_pUserData);
	const auto &norm = mesh->m_normals[iFace * 3 + iVert];
	fvNormOut[0] = norm.x;
	fvNormOut[1] = norm.y;
	fvNormOut[2] = norm.z;
}

static void mikktGetTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert)
{
	LoadedModel::Mesh *mesh = reinterpret_cast<LoadedModel::Mesh *>(pContext->m_pUserData);
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
	LoadedModel::Mesh *mesh = reinterpret_cast<LoadedModel::Mesh *>(pContext->m_pUserData);
	mesh->m_tangents[iFace * 3 + iVert] = glm::vec4(fvTangent[0], fvTangent[1], fvTangent[2], fSign);
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
	eastl::vector<unsigned int> remap(faceCount * 3);
	size_t uniqueVertexCount = meshopt_generateVertexRemapMulti(remap.data(), nullptr, faceCount * 3, faceCount * 3, streams, weights == nullptr ? 4 : 6);

	// fill new index and vertex buffers
	eastl::vector<T> indices(faceCount * 3);
	eastl::vector<glm::vec3> positionsIndexed(uniqueVertexCount);
	eastl::vector<glm::vec3> normalsIndexed(uniqueVertexCount);
	eastl::vector<glm::vec4> tangentsIndexed(uniqueVertexCount);
	eastl::vector<glm::vec2> texCoordsIndexed(uniqueVertexCount);
	eastl::vector<glm::vec4> weightsIndexed;
	eastl::vector<glm::uvec4> jointsIndexed;
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

static bool cookPhysicsMeshes(
	const LoadedModel &model,
	Physics *physics,
	eastl::vector<char> *dataSegment,
	uint32_t *physicsConvexMeshDataOffset,
	uint32_t *physicsConvexMeshDataSize,
	uint32_t *physicsTriangleMeshDataOffset,
	uint32_t *physicsTriangleMeshDataSize)
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
	*physicsConvexMeshDataOffset = static_cast<uint32_t>(dataSegment->size());
	*physicsConvexMeshDataSize = physicsConvexMeshSize;
	dataSegment->insert(dataSegment->end(), physicsConvexMeshData, physicsConvexMeshData + physicsConvexMeshSize);

	*physicsTriangleMeshDataOffset = static_cast<uint32_t>(dataSegment->size());
	*physicsTriangleMeshDataSize = physicsTriangleMeshSize;
	dataSegment->insert(dataSegment->end(), physicsTriangleMeshData, physicsTriangleMeshData + physicsTriangleMeshSize);

	// free memory
	delete[] physicsConvexMeshData;
	delete[] physicsTriangleMeshData;

	return true;
}

bool MeshImporter::importMeshes(size_t count, LoadedModel *models, const char *baseDstPath, const char *sourcePath, Physics *physics) noexcept
{
	SMikkTSpaceInterface mikkTSpaceInterface = {};
	mikkTSpaceInterface.m_getNumFaces = mikktGetNumFaces;
	mikkTSpaceInterface.m_getNumVerticesOfFace = mikktGetNumVerticesOfFace;
	mikkTSpaceInterface.m_getPosition = mikktGetPosition;
	mikkTSpaceInterface.m_getNormal = mikktGetNormal;
	mikkTSpaceInterface.m_getTexCoord = mikktGetTexCoord;
	mikkTSpaceInterface.m_setTSpaceBasic = mikktSetTSpaceBasic;
	mikkTSpaceInterface.m_setTSpace = nullptr;

	auto *assetMgr = AssetManager::get();
	auto &vfs = VirtualFileSystem::get();

	for (size_t i = 0; i < count; ++i)
	{
		auto &model = models[i];
		if (model.m_meshes.empty())
		{
			continue;
		}

		eastl::string dstPath = count > 1 ? (eastl::string(baseDstPath) + "_mesh" + eastl::to_string(i) + ".mesh") : (eastl::string(baseDstPath) + ".mesh");

		MeshAssetData::FileHeader header{};
		header.m_materialSlotCount = static_cast<uint32_t>(model.m_materials.size());

		eastl::vector<MeshAssetData::FileSubMeshHeader> subMeshHeaders;

		eastl::vector<char> dataSegment;

		// cook physics meshes
		cookPhysicsMeshes(model, physics, &dataSegment, &header.m_physicsConvexMeshDataOffset, &header.m_physicsConvexMeshDataSize, &header.m_physicsTriangleMeshDataOffset, &header.m_physicsTriangleMeshDataSize);

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
					header.m_matrixPaletteSize = std::max(header.m_matrixPaletteSize, wi[0] + 1);
					header.m_matrixPaletteSize = std::max(header.m_matrixPaletteSize, wi[1] + 1);
					header.m_matrixPaletteSize = std::max(header.m_matrixPaletteSize, wi[2] + 1);
					header.m_matrixPaletteSize = std::max(header.m_matrixPaletteSize, wi[3] + 1);
				}
			}

			// generate smaller 64k meshes
			{
				glm::vec3 aabbMin = glm::vec3(std::numeric_limits<float>::max());
				glm::vec3 aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
				glm::vec2 uvAabbMin = glm::vec2(std::numeric_limits<float>::max());
				glm::vec2 uvAabbMax = glm::vec2(std::numeric_limits<float>::lowest());

				eastl::vector<glm::vec3> positions;
				eastl::vector<glm::vec3> normals;
				eastl::vector<glm::vec4> tangents;
				eastl::vector<glm::vec2> texCoords;
				eastl::vector<glm::vec4> weights;
				eastl::vector<glm::uvec4> joints;

				// keep track of the number of unique vertices -> we are targeting 16bit indices, so we need to stay below 2^16 - 1 vertices
				eastl::hash_set<Vertex, VertexHash> vertexSet;

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
						eastl::vector<uint16_t> quantizedPositions;
						eastl::vector<uint16_t> quantizedQTangents;
						eastl::vector<uint16_t> quantizedTexCoords;
						eastl::vector<uint32_t> quantizedWeights;
						eastl::vector<uint32_t> quantizedJoints;
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

						MeshAssetData::FileSubMeshHeader subMeshHeader{};
						subMeshHeader.m_dataOffset = static_cast<uint32_t>(dataSegment.size());
						subMeshHeader.m_vertexCount = static_cast<uint32_t>(indexedMesh16.positions.size());
						subMeshHeader.m_indexCount = static_cast<uint32_t>(indexedMesh16.indices.size());
						subMeshHeader.m_materialIndex = static_cast<uint32_t>(mesh.m_materialIndex);
						subMeshHeader.m_flags = !weights.empty() ? 1 : 0;
						subMeshHeader.m_aabbMinX = aabbMin.x;
						subMeshHeader.m_aabbMinY = aabbMin.y;
						subMeshHeader.m_aabbMinZ = aabbMin.z;
						subMeshHeader.m_aabbMaxX = aabbMax.x;
						subMeshHeader.m_aabbMaxY = aabbMax.y;
						subMeshHeader.m_aabbMaxZ = aabbMax.z;
						subMeshHeader.m_uvMinX = uvAabbMin.x;
						subMeshHeader.m_uvMinY = uvAabbMin.y;
						subMeshHeader.m_uvMaxX = uvAabbMax.x;
						subMeshHeader.m_uvMaxY = uvAabbMax.y;

						subMeshHeaders.push_back(subMeshHeader);


						// write to file
						const char *indicesData = (const char *)indexedMesh16.indices.data();
						const char *positionsData = (const char *)indexedMesh16.positions.data();
						const char *normalsData = (const char *)indexedMesh16.normals.data();
						const char *tangentsData = (const char *)indexedMesh16.tangents.data();
						const char *texCoordsData = (const char *)indexedMesh16.texCoords.data();
						dataSegment.insert(dataSegment.end(), indicesData, indicesData + indexedMesh16.indices.size() * sizeof(uint16_t));
						dataSegment.insert(dataSegment.end(), positionsData, positionsData + indexedMesh16.positions.size() * sizeof(glm::vec3));
						dataSegment.insert(dataSegment.end(), normalsData, normalsData + indexedMesh16.normals.size() * sizeof(glm::vec3));
						dataSegment.insert(dataSegment.end(), tangentsData, tangentsData + indexedMesh16.tangents.size() * sizeof(glm::vec4));
						dataSegment.insert(dataSegment.end(), texCoordsData, texCoordsData + indexedMesh16.texCoords.size() * sizeof(glm::vec2));
						if (!weights.empty())
						{
							const char *jointsData = (const char *)quantizedJoints.data();
							const char *weightsData = (const char *)quantizedWeights.data();
							dataSegment.insert(dataSegment.end(), jointsData, jointsData + quantizedJoints.size() * sizeof(uint32_t));
							dataSegment.insert(dataSegment.end(), weightsData, weightsData + quantizedWeights.size() * sizeof(uint32_t));
						}

						++header.m_subMeshCount;

						assert(indexedMesh16.indices.size() % 3 == 0);

						// reset vars
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

						Log::info("Processed SubMesh # %u", (unsigned)header.m_subMeshCount - 1);
					}
				}
			}
		}

		header.m_dataSegmentStart = static_cast<uint32_t>(sizeof(header) + subMeshHeaders.size() * sizeof(subMeshHeaders[0]));

		// compute file size
		{
			header.m_fileSize = sizeof(header);
			header.m_fileSize += (uint32_t)subMeshHeaders.size() * sizeof(subMeshHeaders[0]);
			header.m_fileSize += (uint32_t)dataSegment.size();
		}

		assetMgr->createAsset(MeshAssetData::k_assetType, dstPath.c_str(), sourcePath);

		if (FileHandle fh = vfs.open(dstPath.c_str(), FileMode::WRITE, true))
		{
			vfs.write(fh, sizeof(header), &header);

			for (const auto &sm : subMeshHeaders)
			{
				vfs.write(fh, sizeof(sm), &sm);
			}

			vfs.write(fh, dataSegment.size(), dataSegment.data());

			vfs.close(fh);
		}
		else
		{
			Log::err("Could not open file \"%s\" for writing imported asset data!", dstPath.c_str());
			continue;
		}
	}

	return true;
}