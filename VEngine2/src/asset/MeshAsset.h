#pragma once
#include "Asset.h"
#include "Handles.h"
#include <EASTL/vector.h>
#include "MaterialAsset.h"

/// <summary>
/// AssetData implementation for mesh assets.
/// </summary>
class MeshAsset : public AssetData
{
	friend class MeshAssetHandler;
public:
	static constexpr AssetType k_assetType = "EAFC3928-AFA1-4225-9354-92B3303302CD"_uuid;

	enum class Version : uint32_t
	{
		V_1_0 = 0,
		LATEST = V_1_0,
	};

	struct FileHeader
	{
		char m_magicNumber[8] = { 'V', 'E', 'M', 'E', 'S', 'H', ' ', ' ' };
		Version m_version = Version::V_1_0;
		uint32_t m_fileSize;
		uint32_t m_dataSegmentStart;
		uint32_t m_matrixPaletteSize;
		uint32_t m_physicsConvexMeshDataOffset;
		uint32_t m_physicsConvexMeshDataSize;
		uint32_t m_physicsTriangleMeshDataOffset;
		uint32_t m_physicsTriangleMeshDataSize;
		uint32_t m_subMeshCount;
		uint32_t m_materialAssetIDDataOffset;
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

	explicit MeshAsset(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	const eastl::vector<SubMeshHandle> &getSubMeshhandles() const noexcept { return m_subMeshHandles; }
	const eastl::vector<Asset<MaterialAsset>> &getMaterials() const noexcept { return m_materials; }
	PhysicsConvexMeshHandle getPhysicsConvexMeshhandle() const noexcept { return m_physicsConvexMeshHandle; }
	PhysicsTriangleMeshHandle getPhysicsTriangleMeshhandle() const noexcept { return m_physicsTriangleMeshHandle; }
	bool isSkinned() const noexcept { return m_isSkinned; }
	size_t getMatrixPaletteSize() const noexcept { return m_matrixPaletteSize; }
	void getBoundingSphere(float *result) const noexcept { memcpy(result, m_boundingSphere, sizeof(m_boundingSphere)); };

private:
	eastl::vector<SubMeshHandle> m_subMeshHandles;
	eastl::vector<Asset<MaterialAsset>> m_materials;
	PhysicsConvexMeshHandle m_physicsConvexMeshHandle = {};
	PhysicsTriangleMeshHandle m_physicsTriangleMeshHandle = {};
	size_t m_matrixPaletteSize = 0;
	float m_boundingSphere[4] = {};
	bool m_isSkinned = false;
};