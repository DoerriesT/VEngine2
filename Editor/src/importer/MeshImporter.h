#pragma once

struct LoadedModel;
struct StringID;
typedef StringID AssetID;
class Physics;

namespace MeshImporter
{
	bool importMeshes(size_t count, LoadedModel *models, const char *baseDstPath, const char *sourcePath, Physics *physics, const AssetID *materialAssetIDs, bool cookConvexPhysicsMesh, bool cookTrianglePhysicsMesh) noexcept;
}