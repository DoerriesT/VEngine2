#pragma once
#include "Asset.h"
#include "Handles.h"
#include <EASTL/vector.h>

/// <summary>
/// AssetData implementation for mesh assets.
/// </summary>
class MeshAssetData : public AssetData
{
	friend class MeshAssetHandler;
public:
	static constexpr AssetType k_assetType = "EAFC3928-AFA1-4225-9354-92B3303302CD"_uuid;

	struct FileHeader
	{
		uint32_t m_dataSegmentStart;
		uint32_t m_matrixPaletteSize;
		uint32_t m_physicsConvexMeshDataOffset;
		uint32_t m_physicsConvexMeshDataSize;
		uint32_t m_physicsTriangleMeshDataOffset;
		uint32_t m_physicsTriangleMeshDataSize;
		uint32_t m_subMeshCount;
		uint32_t m_materialSlotCount;
		float m_aabbMinX;
		float m_aabbMinY;
		float m_aabbMinZ;
		float m_aabbMaxX;
		float m_aabbMaxY;
		float m_aabbMaxZ;
	};

	struct FileSubMeshHeader
	{
		uint32_t m_dataOffset;
		uint32_t m_vertexCount;
		uint32_t m_indexCount;
		uint32_t m_materialIndex;
		uint32_t m_flags; // 1 : isSkinned
		float m_aabbMinX;
		float m_aabbMinY;
		float m_aabbMinZ;
		float m_aabbMaxX;
		float m_aabbMaxY;
		float m_aabbMaxZ;
		float m_uvMinX;
		float m_uvMinY;
		float m_uvMaxX;
		float m_uvMaxY;
	};

	explicit MeshAssetData(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	const eastl::vector<SubMeshHandle> &getSubMeshhandles() const noexcept { return m_subMeshHandles; }
	PhysicsConvexMeshHandle getPhysicsConvexMeshhandle() const noexcept { return m_physicsConvexMeshHandle; }
	PhysicsTriangleMeshHandle getPhysicsTriangleMeshhandle() const noexcept { return m_physicsTriangleMeshHandle; }
	bool isSkinned() const noexcept { return m_isSkinned; }
	size_t getMatrixPaletteSize() const noexcept { return m_matrixPaletteSize; }

private:
	eastl::vector<SubMeshHandle> m_subMeshHandles;
	PhysicsConvexMeshHandle m_physicsConvexMeshHandle = {};
	PhysicsTriangleMeshHandle m_physicsTriangleMeshHandle = {};
	size_t m_matrixPaletteSize = 0;
	bool m_isSkinned = false;
};