#pragma once

struct LoadedMaterial;
struct StringID;
typedef StringID AssetID;

namespace MaterialImporter
{
	bool importMaterials(size_t count, LoadedMaterial *materials, const char *baseDstPath, const char *sourcePath, AssetID *resultAssetIDs) noexcept;
}