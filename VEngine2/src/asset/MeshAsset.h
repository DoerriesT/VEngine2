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

	explicit MeshAssetData(const AssetID &assetID) noexcept;
	inline const eastl::vector<SubMeshHandle> &getSubMeshhandles() const noexcept { return m_subMeshHandles; }
	inline PhysicsConvexMeshHandle getPhysicsConvexMeshhandle() const noexcept { return m_physicsConvexMeshHandle; }
	inline PhysicsTriangleMeshHandle getPhysicsTriangleMeshhandle() const noexcept { return m_physicsTriangleMeshHandle; }
	inline bool isSkinned() const noexcept { return m_isSkinned; }
	inline size_t getMatrixPaletteSize() const noexcept { return m_matrixPaletteSize; }

private:
	eastl::vector<SubMeshHandle> m_subMeshHandles;
	PhysicsConvexMeshHandle m_physicsConvexMeshHandle = {};
	PhysicsTriangleMeshHandle m_physicsTriangleMeshHandle = {};
	size_t m_matrixPaletteSize = 0;
	bool m_isSkinned = false;
};