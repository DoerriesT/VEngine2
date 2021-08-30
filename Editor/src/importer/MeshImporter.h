#pragma once

struct LoadedModel;
class Physics;

namespace MeshImporter
{
	bool importMeshes(size_t count, LoadedModel *models, const char *baseDstPath, const char *sourcePath, Physics *physics) noexcept;
}