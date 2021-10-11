#include "AssetMetaDataRegistry.h"
#include <assert.h>
#include "filesystem/VirtualFileSystem.h"

static void fileSystemWatcherCallback(const char *path, FileChangeType changeType, void *userData)
{
	AssetMetaDataRegistry *reg = (AssetMetaDataRegistry *)userData;
	VirtualFileSystem &vfs = VirtualFileSystem::get();

	switch (changeType)
	{
	case FileChangeType::ADDED:
	case FileChangeType::RENAMED_NEW_NAME:
	case FileChangeType::MODIFIED:
	{
		char metaFileName[VirtualFileSystem::k_maxPathLength];
		metaFileName[0] = '\0';
		strcat_s(metaFileName, path);
		strcat_s(metaFileName, ".meta");

		if (vfs.exists(metaFileName))
		{
			AssetID assetID;
			AssetType assetType;
			if (AssetMetaDataRegistry::getAssetIDAndType(metaFileName, &assetID, &assetType))
			{
				LOCK_HOLDER(reg->m_typeToIDsMutex);
				LOCK_HOLDER(reg->m_idToTypeMutex);

				reg->m_assetIDToType[assetID] = assetType;
				eastl::vector<AssetID> &vec = reg->m_assetTypeToIDs[assetType];
				if (eastl::find(vec.begin(), vec.end(), assetID) == vec.end())
				{
					reg->m_assetTypeToIDs[assetType].push_back(assetID);
				}
				reg->m_pathToAssetID[path] = assetID;
			}
		}

		break;
	}

	case FileChangeType::REMOVED:
	case FileChangeType::RENAMED_OLD_NAME:
	{
		LOCK_HOLDER(reg->m_typeToIDsMutex);
		LOCK_HOLDER(reg->m_idToTypeMutex);

		auto it = reg->m_pathToAssetID.find(path);
		if (it != reg->m_pathToAssetID.end())
		{
			AssetID assetID = it->second;
			assert(reg->m_assetIDToType.find(assetID) != reg->m_assetIDToType.end());
			auto assetType = reg->m_assetIDToType[assetID];
			reg->m_assetIDToType.erase(assetID);

			assert(reg->m_assetTypeToIDs.find(assetType) != reg->m_assetTypeToIDs.end());
			eastl::vector<AssetID> &vec = reg->m_assetTypeToIDs[assetType];
			vec.erase(eastl::remove(vec.begin(), vec.end(), assetID), vec.end());
		}
		break;
	}

	default:
		assert(false);
		break;
	}
}

AssetMetaDataRegistry *AssetMetaDataRegistry::get()
{
	static AssetMetaDataRegistry reg;
	return &reg;
}

bool AssetMetaDataRegistry::getAssetIDAndType(const char *metaFilePath, AssetID *assetID, AssetType *assetType) noexcept
{
	VirtualFileSystem &vfs = VirtualFileSystem::get();

	if (FileHandle fh = vfs.open(metaFilePath, FileMode::READ, false))
	{
		char buffer[512]; // first line is the AssetID, which is currently a path (max of 260). second line is AssetType, which is 37 bytes
		const auto bytesRead = vfs.read(fh, sizeof(buffer), buffer);

		if (bytesRead == 0 || buffer[bytesRead - 1] != '\n')
		{
			vfs.close(fh);
			return false;
		}

		for (size_t i = 0; i < bytesRead; ++i)
		{
			if (buffer[i] == '\n')
			{
				buffer[i] = '\0';
			}
		}

		*assetID = AssetID(buffer);
		*assetType = AssetType(buffer + strlen(buffer) + 1);

		vfs.close(fh);

		return true;
	}
	return false;
}

bool AssetMetaDataRegistry::init() noexcept
{
	const char *path = "/assets";

	VirtualFileSystem &vfs = VirtualFileSystem::get();

	vfs.iterateRecursive(path, [&](const FileFindData &ffd)
		{
			auto metaFileName = eastl::string(ffd.m_path) + ".meta";
			if (vfs.exists(metaFileName.c_str()))
			{
				AssetID assetID;
				AssetType assetType;
				if (getAssetIDAndType(metaFileName.c_str(), &assetID, &assetType))
				{
					m_assetIDToType[assetID] = assetType;
					m_assetTypeToIDs[assetType].push_back(assetID);
					m_pathToAssetID[ffd.m_path] = assetID;
				}
			}

			return true;
		});

	m_watcherHandle = VirtualFileSystem::get().openFileSystemWatcher(path, fileSystemWatcherCallback, this);
	return m_watcherHandle != NULL_FILE_SYSTEM_WATCHER_HANDLE;
}

void AssetMetaDataRegistry::shutdown() noexcept
{
	VirtualFileSystem::get().closeFileSystemWatcher(m_watcherHandle);
}

bool AssetMetaDataRegistry::getAssetIDs(const AssetType &assetType, size_t *count, AssetID *resultArray) noexcept
{
	assert(count);
	LOCK_HOLDER(m_typeToIDsMutex);

	auto it = m_assetTypeToIDs.find(assetType);
	if (it == m_assetTypeToIDs.end())
	{
		*count = 0;
		return true;
	}

	*count = it->second.size();

	// caller wants the data copied into the result array
	if (resultArray)
	{
		bool wroteAll = *count >= it->second.size();
		memcpy(resultArray, it->second.data(), it->second.size() * sizeof(it->second[0]));

		return wroteAll;
	}

	return true;
}

bool AssetMetaDataRegistry::getAssetIDAt(const AssetType &assetType, size_t idx, AssetID *resultAssetID) noexcept
{
	assert(resultAssetID);
	LOCK_HOLDER(m_typeToIDsMutex);

	auto it = m_assetTypeToIDs.find(assetType);
	if (it == m_assetTypeToIDs.end())
	{
		return false;
	}

	// make sure the index is in bounds
	if (idx < it->second.size())
	{
		*resultAssetID = it->second[idx];
		return true;
	}

	return false;
}

bool AssetMetaDataRegistry::getAssetType(const AssetID &assetID, AssetType *resultAssetType) noexcept
{
	assert(resultAssetType);
	LOCK_HOLDER(m_idToTypeMutex);

	auto it = m_assetIDToType.find(assetID);
	if (it == m_assetIDToType.end())
	{
		return false;
	}

	*resultAssetType = it->second;

	return true;
}

bool AssetMetaDataRegistry::isRegistered(const AssetID &assetID) noexcept
{
	LOCK_HOLDER(m_idToTypeMutex);
	return  m_assetIDToType.find(assetID) != m_assetIDToType.end();
}
