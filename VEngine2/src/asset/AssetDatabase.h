#pragma once
#include <filesystem>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include "Asset.h"

struct MeshImportOptions
{
	bool m_mergeByMaterial;
	bool m_invertTexCoordY;
};

struct AssetImportOptions
{
	MeshImportOptions m_meshOptions;
};

struct AssetDatabaseEntry
{
	AssetID m_assetID;
	AssetType m_assetType;
	std::filesystem::path m_path;
	std::filesystem::path m_sourcePath;
	AssetImportOptions m_importOptions;
};

class AssetDatabase
{
public:
	void loadFromFile(const char *path) noexcept;
	void saveToFile(const char *path) noexcept;
	void clear() noexcept;
	bool createEntry(const AssetDatabaseEntry &entry) noexcept;
	bool removeEntry(const AssetID &assetID) noexcept;
	const AssetDatabaseEntry *getEntry(const AssetID &assetID) const noexcept;

private:
	eastl::vector<AssetDatabaseEntry> m_entries;
	eastl::hash_map<uint64_t, size_t> m_assetIDToEntryIdxMap;
};