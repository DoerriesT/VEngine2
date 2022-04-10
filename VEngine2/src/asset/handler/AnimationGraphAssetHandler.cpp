#include "AnimationGraphAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "asset/AnimationGraphAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"

static AssetManager *s_assetManager = nullptr;
static AnimationGraphAssetHandler s_animationGraphAssetHandler;

void AnimationGraphAssetHandler::init(AssetManager *assetManager) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		assetManager->registerAssetHandler(AnimationGraphAsset::k_assetType, &s_animationGraphAssetHandler);
	}
}

void AnimationGraphAssetHandler::shutdown() noexcept
{
	s_animationGraphAssetHandler = {};
	s_assetManager = nullptr;
}

AssetData *AnimationGraphAssetHandler::createEmptyAssetData(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != AnimationGraphAsset::k_assetType)
	{
		Log::warn("AnimationGraphAssetHandler: Tried to call createEmptyAssetData() on a non-animation graph asset!");
		return nullptr;
	}

	return new AnimationGraphAsset(assetID);
}

bool AnimationGraphAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != AnimationGraphAsset::k_assetType)
	{
		Log::warn("AnimationGraphAssetHandler: Tried to call loadAssetData() on a non-animation clip asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load asset
	{
		if (!VirtualFileSystem::get().exists(path) || VirtualFileSystem::get().isDirectory(path))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("AnimationGraphAssetHandler: Animation clip asset data file \"%s\" could not be found!", path);
			return false;
		}

		auto fileSize = VirtualFileSystem::get().size(path);

		if (fileSize < sizeof(AnimationGraphAsset::FileHeader))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("AnimationGraphAssetHandler: Animation clip asset data file \"%s\" has a wrong format! (Too small to contain header data)", path);
			return false;
		}

		eastl::vector<char> fileData(fileSize);

		bool success = false;

		if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
		{
			const char *data = fileData.data();

			// we checked earlier that the header actually fits inside the file
			AnimationGraphAsset::FileHeader header = *reinterpret_cast<const AnimationGraphAsset::FileHeader *>(data);

			// check the magic number
			AnimationGraphAsset::FileHeader defaultHeader{};
			if (memcmp(header.m_magicNumber, defaultHeader.m_magicNumber, sizeof(defaultHeader.m_magicNumber)) != 0)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("AnimationGraphAssetHandler: Animation clip asset data file \"%s\" has a wrong format! (Magic number does not match)", path);
				return false;
			}

			// check the version
			if (header.m_version != AnimationGraphAsset::Version::LATEST)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("AnimationGraphAssetHandler: Animation clip asset data file \"%s\" has unsupported version \"%u\"!", path, (unsigned)header.m_version);
				return false;
			}

			// check the file size
			if (header.m_fileSize > fileSize)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("AnimationGraphAssetHandler: File size (%u) specified in header of animation clip asset data file \"%s\" is greater than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
				return false;
			}

			if (header.m_fileSize < fileSize)
			{
				Log::warn("AnimationGraphAssetHandler: File size (%u) specified in header of animation clip asset data file \"%s\" is less than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
			}

			data += sizeof(header);
			size_t curFileOffset = 0;

			AnimationGraphCreateInfo animGraphCreateInfo{};
			{
				animGraphCreateInfo.m_rootNodeIndex = header.m_rootNodeIndex;
				animGraphCreateInfo.m_nodeCount = header.m_nodeCount;
				animGraphCreateInfo.m_parameterCount = header.m_parameterCount;
				animGraphCreateInfo.m_animationClipCount = header.m_animationClipAssetCount;

				// load controller script
				AssetID controllerScriptAssetID = AssetID(data);
				data += strlen(data) + 1;
				animGraphCreateInfo.m_controllerScript = s_assetManager->getAsset<ScriptAsset>(controllerScriptAssetID);

				// compute required size of single memory allocation
				size_t requiredMemorySize = 0;

				// nodes
				const size_t nodesOffset = requiredMemorySize;
				const size_t nodesSize = header.m_nodeCount * sizeof(AnimationGraphNode);
				requiredMemorySize += nodesSize;

				// params
				requiredMemorySize = util::alignPow2Up(requiredMemorySize, alignof(AnimationGraphParameter));
				const size_t paramsOffset = requiredMemorySize;
				const size_t paramsSize = header.m_parameterCount * sizeof(AnimationGraphParameter);
				requiredMemorySize += paramsSize;

				// clips
				requiredMemorySize = util::alignPow2Up(requiredMemorySize, alignof(Asset<AnimationClipAsset>));
				const size_t clipsOffset = requiredMemorySize;
				const size_t clipsSize = header.m_animationClipAssetCount * sizeof(Asset<AnimationClipAsset>);
				requiredMemorySize += clipsSize;

				// do a single allocation for all graph data
				char *memory = new char[requiredMemorySize];
				assert(memory);

				// memcpy/placement new the graph data into the single allocation
				memcpy(memory + nodesOffset, data, nodesSize);
				data += nodesSize;
				memcpy(memory + paramsOffset, data, paramsSize);
				data += paramsSize;
				for (size_t i = 0; i < header.m_animationClipAssetCount; ++i)
				{
					char *ptr = memory + clipsOffset + i * sizeof(Asset<AnimationClipAsset>);
					AssetID clipAssetID = AssetID(data);
					data += strlen(data) + 1;
					new (ptr) Asset<AnimationClipAsset>(s_assetManager->getAsset<AnimationClipAsset>(clipAssetID));
				}

				animGraphCreateInfo.m_nodes = reinterpret_cast<AnimationGraphNode *>(memory + nodesOffset);
				animGraphCreateInfo.m_parameters = reinterpret_cast<AnimationGraphParameter *>(memory + paramsOffset);
				animGraphCreateInfo.m_animationClips = reinterpret_cast<Asset<AnimationClipAsset> *>(memory + clipsOffset);
				animGraphCreateInfo.m_memory = memory;
			}

			static_cast<AnimationGraphAsset *>(assetData)->m_animationGraph = AnimationGraph(animGraphCreateInfo);

			success = true;
		}

		if (!success)
		{
			Log::warn("AnimationGraphAssetHandler: Failed to load animation clip asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void AnimationGraphAssetHandler::destroyAssetData(AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != AnimationGraphAsset::k_assetType)
	{
		Log::warn("AnimationGraphAssetHandler: Tried to call destroyAssetData() on a non-animation graph asset!");
		return;
	}

	delete assetData;
}
