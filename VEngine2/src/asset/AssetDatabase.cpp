#include "AssetDatabase.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <filesystem>

void AssetDatabase::loadFromFile(const char *path) noexcept
{
	clear();

	if (std::filesystem::exists(path))
	{
		nlohmann::json json;
		{
			std::ifstream file(path);
			file >> json;
		}

		for (const auto &je : json["entries"])
		{
			AssetDatabaseEntry entry;
			entry.m_assetID = AssetID(je["assetID"].get<std::string>().c_str());
			entry.m_assetType = AssetType(je["assetType"].get<std::string>().c_str());
			entry.m_path = je["path"].get<std::string>().c_str();
			entry.m_sourcePath = je["sourcePath"].get<std::string>().c_str();
			entry.m_importOptions.m_meshOptions.m_mergeByMaterial = je["meshMergeByMaterial"].get<bool>();
			entry.m_importOptions.m_meshOptions.m_invertTexCoordY = je["meshInvertTexCoordY"].get<bool>();

			createEntry(entry);
		}
	}
}

void AssetDatabase::saveToFile(const char *path) noexcept
{
	nlohmann::json json;
	json["entries"] = nlohmann::json::array();

	for (const auto &e : m_entries)
	{
		char assetTypeStr[37];
		e.m_assetType.toString(assetTypeStr);
		json["entries"].push_back(
			{
				{"assetID", e.m_assetID.m_string },
				{"assetType", assetTypeStr },
				{"path", e.m_path.u8string() },
				{"sourcePath", e.m_sourcePath.u8string() },
				{"meshMergeByMaterial", e.m_importOptions.m_meshOptions.m_mergeByMaterial },
				{"meshInvertTexCoordY", e.m_importOptions.m_meshOptions.m_invertTexCoordY },
			}
		);
	}

	std::ofstream file(path, std::ios::out | std::ios::trunc);
	file << std::setw(4) << json << std::endl;
}

void AssetDatabase::clear() noexcept
{
	m_entries.clear();
	m_assetIDToEntryIdxMap.clear();
}

bool AssetDatabase::createEntry(const AssetDatabaseEntry &entry) noexcept
{
	auto it = m_assetIDToEntryIdxMap.find(entry.m_assetID.m_hash);
	if (it != m_assetIDToEntryIdxMap.end())
	{
		m_entries[it->second] = entry;
		return true;
	}
	else
	{
		m_assetIDToEntryIdxMap[entry.m_assetID.m_hash] = m_entries.size();
		m_entries.push_back(entry);
		return false;
	}
}

bool AssetDatabase::removeEntry(const AssetID &assetID) noexcept
{
	auto it = m_assetIDToEntryIdxMap.find(assetID.m_hash);
	if (it != m_assetIDToEntryIdxMap.end())
	{
		size_t idx = it->second;
		size_t lastIdx = m_entries.size() - 1;

		// swap with last element
		if (idx != lastIdx)
		{
			// move last element into deleted elem
			m_entries[idx] = eastl::move(m_entries[lastIdx]);
			// set last element hashmap entry to point to new position
			m_assetIDToEntryIdxMap[m_entries[idx].m_assetID.m_hash] = idx;
		}

		// remove last element
		m_entries.pop_back();
		// remove deleted element hashmap entry
		m_assetIDToEntryIdxMap.erase(it);

		return true;
	}
	return false;
}

const AssetDatabaseEntry *AssetDatabase::getEntry(const AssetID &assetID) const noexcept
{
	auto it = m_assetIDToEntryIdxMap.find(assetID.m_hash);
	if (it != m_assetIDToEntryIdxMap.end())
	{
		return &m_entries[it->second];
	}
	return nullptr;
}
