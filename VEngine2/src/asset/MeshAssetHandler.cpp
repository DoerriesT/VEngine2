#include "MeshAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "graphics/Renderer.h"
#include "physics/Physics.h"
#include "MeshAsset.h"
#include "AssetManager.h"
#include <nlohmann/json.hpp>
#include <fstream>

static AssetManager *s_assetManager = nullptr;
static MeshAssetHandler s_meshAssetHandler;

void MeshAssetHandler::init(AssetManager *assetManager, Renderer *renderer, Physics *physics) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_meshAssetHandler.m_renderer = renderer;
		s_meshAssetHandler.m_physics = physics;
		assetManager->registerAssetHandler(MeshAssetData::k_assetType, &s_meshAssetHandler);
	}
}

void MeshAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(MeshAssetData::k_assetType);
		s_meshAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *MeshAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != MeshAssetData::k_assetType)
	{
		Log::warn("MeshAssetHandler: Tried to call createAsset on a non-mesh asset!");
		return nullptr;
	}

	return new MeshAssetData(assetID);
}

bool MeshAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != MeshAssetData::k_assetType)
	{
		Log::warn("MeshAssetHandler: Tried to call loadAssetData on a non-mesh asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load asset
	{
		auto *meshAssetData = static_cast<MeshAssetData *>(assetData);

		nlohmann::json info;
		{
			std::ifstream file(std::string(path) + ".info");
			file >> info;
		}

		// load mesh
		{
			size_t fileSize = 0;
			char *meshData = util::readBinaryFile(info["meshFile"].get<std::string>().c_str(), &fileSize);

			// physics meshes
			{
				// convex mesh
				uint32_t convexMeshSize = 0;
				uint32_t convexMeshOffset = 0;
				if (info.contains("physicsConvexMeshDataSize"))
				{
					convexMeshSize = info["physicsConvexMeshDataSize"].get<uint32_t>();
				}
				if (info.contains("physicsConvexMeshDataOffset"))
				{
					convexMeshOffset = info["physicsConvexMeshDataOffset"].get<uint32_t>();
				}

				if (convexMeshSize > 0)
				{
					meshAssetData->m_physicsConvexMeshHandle = m_physics->createConvexMesh(convexMeshSize, meshData + convexMeshOffset);
				}


				// triangle mesh
				uint32_t triangleMeshSize = 0;
				uint32_t triangleMeshOffset = 0;
				if (info.contains("physicsTriangleMeshDataSize"))
				{
					triangleMeshSize = info["physicsTriangleMeshDataSize"].get<uint32_t>();
				}
				if (info.contains("physicsTriangleMeshDataOffset"))
				{
					triangleMeshOffset = info["physicsTriangleMeshDataOffset"].get<uint32_t>();
				}

				if (triangleMeshOffset > 0)
				{
					meshAssetData->m_physicsTriangleMeshHandle = m_physics->createTriangleMesh(triangleMeshSize, meshData + triangleMeshOffset);
				}
			}

			const uint32_t subMeshCount = static_cast<uint32_t>(info["subMeshes"].size());
			std::vector<SubMeshCreateInfo> subMeshes;
			auto &subMeshHandles = meshAssetData->m_subMeshHandles;

			subMeshes.reserve(subMeshCount);
			subMeshHandles.resize(subMeshCount);

			for (const auto &subMeshInfo : info["subMeshes"])
			{
				size_t dataOffset = subMeshInfo["dataOffset"].get<size_t>();

				SubMeshCreateInfo subMesh{};
				subMesh.m_minCorner[0] = subMeshInfo["minCorner"][0].get<float>();
				subMesh.m_minCorner[1] = subMeshInfo["minCorner"][1].get<float>();
				subMesh.m_minCorner[2] = subMeshInfo["minCorner"][2].get<float>();
				subMesh.m_maxCorner[0] = subMeshInfo["maxCorner"][0].get<float>();
				subMesh.m_maxCorner[1] = subMeshInfo["maxCorner"][1].get<float>();
				subMesh.m_maxCorner[2] = subMeshInfo["maxCorner"][2].get<float>();
				subMesh.m_minTexCoord[0] = subMeshInfo["texCoordMin"][0].get<float>();
				subMesh.m_minTexCoord[1] = subMeshInfo["texCoordMin"][1].get<float>();
				subMesh.m_maxTexCoord[0] = subMeshInfo["texCoordMax"][0].get<float>();
				subMesh.m_maxTexCoord[1] = subMeshInfo["texCoordMax"][1].get<float>();
				subMesh.m_vertexCount = subMeshInfo["vertexCount"].get<uint32_t>();
				subMesh.m_indexCount = subMeshInfo["indexCount"].get<uint32_t>();

				subMesh.m_positions = (float *)(meshData + dataOffset);
				dataOffset += subMesh.m_vertexCount * sizeof(float) * 3;

				subMesh.m_normals = (float *)(meshData + dataOffset);
				dataOffset += subMesh.m_vertexCount * sizeof(float) * 3;

				subMesh.m_tangents = (float *)(meshData + dataOffset);
				dataOffset += subMesh.m_vertexCount * sizeof(float) * 4;

				subMesh.m_texCoords = (float *)(meshData + dataOffset);
				dataOffset += subMesh.m_vertexCount * sizeof(float) * 2;

				subMesh.m_indices = (uint16_t *)(meshData + dataOffset);
				dataOffset += subMesh.m_indexCount * sizeof(uint16_t);

				subMeshes.push_back(subMesh);
			}

			m_renderer->createSubMeshes(subMeshCount, subMeshes.data(), subMeshHandles.data());

			delete[] meshData;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void MeshAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != MeshAssetData::k_assetType)
	{
		Log::warn("MeshAssetHandler: Tried to call destroyAsset on a non-mesh asset!");
		return;
	}

	delete assetData;
}
