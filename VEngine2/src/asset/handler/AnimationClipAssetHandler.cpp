#include "AnimationClipAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "asset/AnimationClipAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"

static AssetManager *s_assetManager = nullptr;
static AnimationClipAssetHandler s_animationClipAssetHandler;

void AnimationClipAssetHandler::init(AssetManager *assetManager) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		assetManager->registerAssetHandler(AnimationClipAsset::k_assetType, &s_animationClipAssetHandler);
	}
}

void AnimationClipAssetHandler::shutdown() noexcept
{
	s_animationClipAssetHandler = {};
	s_assetManager = nullptr;
}

AssetData *AnimationClipAssetHandler::createEmptyAssetData(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != AnimationClipAsset::k_assetType)
	{
		Log::warn("AnimationClipAssetHandler: Tried to call createEmptyAssetData() on a non-animation clip asset!");
		return nullptr;
	}

	return new AnimationClipAsset(assetID);
}

bool AnimationClipAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != AnimationClipAsset::k_assetType)
	{
		Log::warn("AnimationClipAssetHandler: Tried to call loadAssetData() on a non-animation clip asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load asset
	{
		if (!VirtualFileSystem::get().exists(path) || VirtualFileSystem::get().isDirectory(path))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("AnimationClipAssetHandler: Animation clip asset data file \"%s\" could not be found!", path);
			return false;
		}

		auto fileSize = VirtualFileSystem::get().size(path);

		if (fileSize < sizeof(AnimationClipAsset::FileHeader))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("AnimationClipAssetHandler: Animation clip asset data file \"%s\" has a wrong format! (Too small to contain header data)", path);
			return false;
		}

		eastl::vector<char> fileData(fileSize);

		bool success = false;

		if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
		{
			const char *data = fileData.data();

			// we checked earlier that the header actually fits inside the file
			AnimationClipAsset::FileHeader header = *reinterpret_cast<const AnimationClipAsset::FileHeader *>(data);

			// check the magic number
			AnimationClipAsset::FileHeader defaultHeader{};
			if (memcmp(header.m_magicNumber, defaultHeader.m_magicNumber, sizeof(defaultHeader.m_magicNumber)) != 0)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("AnimationClipAssetHandler: Animation clip asset data file \"%s\" has a wrong format! (Magic number does not match)", path);
				return false;
			}

			// check the version
			if (header.m_version != AnimationClipAsset::Version::LATEST)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("AnimationClipAssetHandler: Animation clip asset data file \"%s\" has unsupported version \"%u\"!", path, (unsigned)header.m_version);
				return false;
			}

			// check the file size
			if (header.m_fileSize > fileSize)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("AnimationClipAssetHandler: File size (%u) specified in header of animation clip asset data file \"%s\" is greater than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
				return false;
			}

			if (header.m_fileSize < fileSize)
			{
				Log::warn("AnimationClipAssetHandler: File size (%u) specified in header of animation clip asset data file \"%s\" is less than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
			}

			data += sizeof(header);
			size_t curFileOffset = 0;

			AnimationClipCreateInfo animClipCreateInfo{};
			{
				animClipCreateInfo.m_jointCount = header.m_jointCount;
				animClipCreateInfo.m_duration = header.m_duration;

				const AnimationClipJointInfo *jointInfo = reinterpret_cast<const AnimationClipJointInfo *>(data + curFileOffset);

				assert((sizeof(header) + curFileOffset + animClipCreateInfo.m_jointCount * sizeof(AnimationClipJointInfo)) < fileSize);

				const auto &lastJointInfo = jointInfo[animClipCreateInfo.m_jointCount - 1];
				const uint32_t translationCount = lastJointInfo.m_translationArrayOffset + lastJointInfo.m_translationFrameCount;
				const uint32_t rotationCount = lastJointInfo.m_rotationArrayOffset + lastJointInfo.m_rotationFrameCount;
				const uint32_t scaleCount = lastJointInfo.m_scaleArrayOffset + lastJointInfo.m_scaleFrameCount;

				size_t memorySize = 0;
				memorySize += animClipCreateInfo.m_jointCount * sizeof(AnimationClipJointInfo); // per joint info
				memorySize += (translationCount + rotationCount + scaleCount) * sizeof(float); // time keys
				memorySize += translationCount * sizeof(float) * 3; // translation data
				memorySize += rotationCount * sizeof(float) * 4; // rotation data
				memorySize += scaleCount * sizeof(float) * 1; // scale data

				assert((curFileOffset + memorySize) <= fileSize);

				// allocate memory
				char *memory = new char[memorySize];
				assert(memory);

				// copy data to our allocation
				memcpy(memory, data + curFileOffset, memorySize);

				// set up pointers
				animClipCreateInfo.m_memory = memory;
				size_t curMemOffset = 0;

				animClipCreateInfo.m_perJointInfo = reinterpret_cast<const AnimationClipJointInfo *>(memory + curMemOffset);
				curMemOffset += animClipCreateInfo.m_jointCount * sizeof(AnimationClipJointInfo);

				animClipCreateInfo.m_translationTimeKeys = reinterpret_cast<const float *>(memory + curMemOffset);
				curMemOffset += translationCount * sizeof(float);

				animClipCreateInfo.m_rotationTimeKeys = reinterpret_cast<const float *>(memory + curMemOffset);
				curMemOffset += rotationCount * sizeof(float);

				animClipCreateInfo.m_scaleTimeKeys = reinterpret_cast<const float *>(memory + curMemOffset);
				curMemOffset += scaleCount * sizeof(float);

				animClipCreateInfo.m_translationData = reinterpret_cast<const float *>(memory + curMemOffset);
				curMemOffset += translationCount * sizeof(float) * 3;

				animClipCreateInfo.m_rotationData = reinterpret_cast<const float *>(memory + curMemOffset);
				curMemOffset += rotationCount * sizeof(float) * 4;

				animClipCreateInfo.m_scaleData = reinterpret_cast<const float *>(memory + curMemOffset);
				curMemOffset += scaleCount * sizeof(float) * 1;
			}

			static_cast<AnimationClipAsset *>(assetData)->m_animationClip = AnimationClip(animClipCreateInfo);

			success = true;
		}

		if (!success)
		{
			Log::warn("AnimationClipAssetHandler: Failed to load animation clip asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void AnimationClipAssetHandler::destroyAssetData(AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != AnimationClipAsset::k_assetType)
	{
		Log::warn("AnimationClipAssetHandler: Tried to call destroyAssetData() on a non-animation clip asset!");
		return;
	}

	delete assetData;
}
