#include "MaterialImporter.h"
#include <nlohmann/json.hpp>
#include <EASTL/string.h>
#include <asset/AssetManager.h>
#include <asset/MaterialAsset.h>
#include <filesystem/VirtualFileSystem.h>
#include <iomanip>
#include <sstream>
#include "loader/LoadedModel.h"
#include <Log.h>

bool MaterialImporter::importMaterials(size_t count, LoadedMaterial *materials, const char *baseDstPath, const char *sourcePath, AssetID *resultAssetIDs) noexcept
{
	auto *assetMgr = AssetManager::get();
	auto &vfs = VirtualFileSystem::get();

	for (size_t i = 0; i < count; ++i)
	{
		const auto &mat = materials[i];

		nlohmann::json j;
		j["name"] = mat.m_name.c_str();
		j["alphaMode"] = (int)mat.m_alpha;
		j["albedo"] = { mat.m_albedoFactor[0], mat.m_albedoFactor[1], mat.m_albedoFactor[2] };
		j["metalness"] = mat.m_metalnessFactor;
		j["roughness"] = mat.m_roughnessFactor;
		j["emissive"] = { mat.m_emissiveFactor[0], mat.m_emissiveFactor[1], mat.m_emissiveFactor[2] };
		j["opacity"] = mat.m_opacity;
		j["albedoTexture"] = mat.m_albedoTexture.c_str();
		j["normalTexture"] = mat.m_normalTexture.c_str();
		j["metalnessTexture"] = mat.m_metalnessTexture.c_str();
		j["roughnessTexture"] = mat.m_roughnessTexture.c_str();
		j["occlusionTexture"] = mat.m_occlusionTexture.c_str();
		j["emissiveTexture"] = mat.m_emissiveTexture.c_str();
		j["displacementTexture"] = mat.m_displacementTexture.c_str();

		std::string cleanedName = mat.m_name.c_str();
		cleanedName.erase(std::remove_if(cleanedName.begin(), cleanedName.end(), [](auto c)
			{
				switch (c)
				{
				case '\\':
				case '/':
				case ':':
				case '*':
				case '?':
				case '"':
				case '<':
				case '>':
				case '|':
					return true;
				default:
					return false;
				}
			}), cleanedName.end());

		std::string dstPath = count > 1 ? (std::string(baseDstPath) + "_" + cleanedName + ".mat") : std::string(baseDstPath) + ".mat";
	
		std::stringstream fileData;
		fileData << std::setw(4) << j << std::endl;
		auto fileDataStr = fileData.str();

		auto assetID = assetMgr->createAsset(MaterialAsset::k_assetType, dstPath.c_str(), sourcePath);
		resultAssetIDs[i] = assetID;

		if (FileHandle fh = vfs.open(dstPath.c_str(), FileMode::WRITE, true))
		{
			vfs.write(fh, fileDataStr.length(), fileDataStr.c_str());
			vfs.close(fh);
		}
		else
		{
			Log::err("Could not open file \"%s\" for writing imported asset data!", dstPath.c_str());
			continue;
		}
	}
	return true;
}
