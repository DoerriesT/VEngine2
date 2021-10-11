#include "MeshAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "graphics/Renderer.h"
#include "physics/Physics.h"
#include "asset/MeshAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"

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
	s_meshAssetHandler = {};
	s_assetManager = nullptr;
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

		// load mesh
		{
			if (!VirtualFileSystem::get().exists(path) || VirtualFileSystem::get().isDirectory(path))
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("MeshAssetHandler: Mesh asset data file \"%s\" could not be found!", path);
				return false;
			}

			auto fileSize = VirtualFileSystem::get().size(path);

			if (fileSize < sizeof(MeshAssetData::FileHeader))
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("MeshAssetHandler: Mesh asset data file \"%s\" has a wrong format! (Too small to contain header data)", path);
				return false;
			}

			eastl::vector<char> fileData(fileSize);

			if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
			{
				const char *data = fileData.data();

				// we checked earlier that the header actually fits inside the file
				MeshAssetData::FileHeader header = *reinterpret_cast<const MeshAssetData::FileHeader *>(data);

				// check the magic number
				MeshAssetData::FileHeader defaultHeader{};
				if (memcmp(header.m_magicNumber, defaultHeader.m_magicNumber, sizeof(defaultHeader.m_magicNumber)) != 0)
				{
					assetData->setAssetStatus(AssetStatus::ERROR);
					Log::err("MeshAssetHandler: Mesh asset data file \"%s\" has a wrong format! (Magic number does not match)", path);
					return false;
				}

				// check the version
				if (header.m_version != MeshAssetData::Version::LATEST)
				{
					assetData->setAssetStatus(AssetStatus::ERROR);
					Log::err("MeshAssetHandler: Mesh asset data file \"%s\" has unsupported version \"%u\"!", path, (unsigned)header.m_version);
					return false;
				}

				// check the file size
				if (header.m_fileSize > fileSize)
				{
					assetData->setAssetStatus(AssetStatus::ERROR);
					Log::err("MeshAssetHandler: File size (%u) specified in header of mesh asset data file \"%s\" is greater than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
					return false;
				}

				if (header.m_fileSize < fileSize)
				{
					Log::warn("MeshAssetHandler: File size (%u) specified in header of mesh asset data file \"%s\" is less than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
				}

				const char *dataSegment = fileData.data() + header.m_dataSegmentStart;

				meshAssetData->m_matrixPaletteSize = header.m_matrixPaletteSize;

				data += sizeof(header);

				// physics meshes
				{
					if (header.m_physicsConvexMeshDataSize > 0)
					{
						meshAssetData->m_physicsConvexMeshHandle = m_physics->createConvexMesh(header.m_physicsConvexMeshDataSize, dataSegment + header.m_physicsConvexMeshDataOffset);
					}

					if (header.m_physicsTriangleMeshDataSize > 0)
					{
						meshAssetData->m_physicsTriangleMeshHandle = m_physics->createTriangleMesh(header.m_physicsTriangleMeshDataSize, dataSegment + header.m_physicsTriangleMeshDataOffset);
					}
				}

				// graphics submeshes
				{
					eastl::vector<SubMeshCreateInfo> subMeshes;
					subMeshes.reserve(header.m_subMeshCount);
					meshAssetData->m_subMeshHandles.resize(header.m_subMeshCount);

					for (size_t i = 0; i < header.m_subMeshCount; ++i)
					{
						const auto &subMeshHeader = *reinterpret_cast<const MeshAssetData::FileSubMeshHeader *>(data);
						data += sizeof(subMeshHeader);

						SubMeshCreateInfo subMesh{};
						subMesh.m_minCorner[0] = subMeshHeader.m_aabbMinX;
						subMesh.m_minCorner[1] = subMeshHeader.m_aabbMinY;
						subMesh.m_minCorner[2] = subMeshHeader.m_aabbMinZ;
						subMesh.m_maxCorner[0] = subMeshHeader.m_aabbMaxX;
						subMesh.m_maxCorner[1] = subMeshHeader.m_aabbMaxY;
						subMesh.m_maxCorner[2] = subMeshHeader.m_aabbMaxZ;
						subMesh.m_minTexCoord[0] = subMeshHeader.m_uvMinX;
						subMesh.m_minTexCoord[1] = subMeshHeader.m_uvMinY;
						subMesh.m_maxTexCoord[0] = subMeshHeader.m_uvMaxX;
						subMesh.m_maxTexCoord[1] = subMeshHeader.m_uvMaxY;
						subMesh.m_vertexCount = subMeshHeader.m_vertexCount;
						subMesh.m_indexCount = subMeshHeader.m_indexCount;

						const char *subMeshData = dataSegment + subMeshHeader.m_dataOffset;

						size_t curOffset = 0;

						subMesh.m_indices = (uint16_t *)(subMeshData + curOffset);
						curOffset += subMesh.m_indexCount * sizeof(uint16_t);

						subMesh.m_positions = (float *)(subMeshData + curOffset);
						curOffset += subMesh.m_vertexCount * sizeof(float) * 3;

						subMesh.m_normals = (float *)(subMeshData + curOffset);
						curOffset += subMesh.m_vertexCount * sizeof(float) * 3;

						subMesh.m_tangents = (float *)(subMeshData + curOffset);
						curOffset += subMesh.m_vertexCount * sizeof(float) * 4;

						subMesh.m_texCoords = (float *)(subMeshData + curOffset);
						curOffset += subMesh.m_vertexCount * sizeof(float) * 2;


						if (subMeshHeader.m_flags & 1) // skinned
						{
							subMesh.m_jointIndices = (uint32_t *)(subMeshData + curOffset);
							curOffset += subMesh.m_vertexCount * sizeof(uint32_t) * 2;

							subMesh.m_jointWeights = (uint32_t *)(subMeshData + curOffset);
							curOffset += subMesh.m_vertexCount * sizeof(uint32_t);

							meshAssetData->m_isSkinned = true;
						}

						subMeshes.push_back(subMesh);
					}

					m_renderer->createSubMeshes(static_cast<uint32_t>(subMeshes.size()), subMeshes.data(), meshAssetData->m_subMeshHandles.data());
				}
			}
			else
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("MeshAssetHandler: Failed to open asset data file \"%s\"!", path);
				return false;
			}
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
