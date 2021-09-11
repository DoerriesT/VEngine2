#include "SkeletonAssetHandler.h"
#include <assert.h>
#include "Log.h"
#include "utility/Utility.h"
#include "animation/AnimationSystem.h"
#include "asset/SkeletonAsset.h"
#include "asset/AssetManager.h"
#include "filesystem/VirtualFileSystem.h"
#include <glm/mat4x4.hpp>

static AssetManager *s_assetManager = nullptr;
static SkeletonAssetHandler s_skeletonAssetHandler;

void SkeletonAssetHandler::init(AssetManager *assetManager, AnimationSystem *animationSystem) noexcept
{
	if (s_assetManager == nullptr)
	{
		s_assetManager = assetManager;
		s_skeletonAssetHandler.m_animationSystem = animationSystem;
		assetManager->registerAssetHandler(SkeletonAssetData::k_assetType, &s_skeletonAssetHandler);
	}
}

void SkeletonAssetHandler::shutdown() noexcept
{
	if (s_assetManager != nullptr)
	{
		s_assetManager->unregisterAssetHandler(SkeletonAssetData::k_assetType);
		s_skeletonAssetHandler = {};
		s_assetManager = nullptr;
	}
}

AssetData *SkeletonAssetHandler::createAsset(const AssetID &assetID, const AssetType &assetType) noexcept
{
	if (assetType != SkeletonAssetData::k_assetType)
	{
		Log::warn("SkeletonAssetHandler: Tried to call createAsset() on a non-skeleton asset!");
		return nullptr;
	}

	return new SkeletonAssetData(assetID);
}

bool SkeletonAssetHandler::loadAssetData(AssetData *assetData, const char *path) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != SkeletonAssetData::k_assetType)
	{
		Log::warn("SkeletonAssetHandler: Tried to call loadAssetData() on a non-skeleton asset!");
		return false;
	}

	assetData->setAssetStatus(AssetStatus::LOADING);

	// load asset
	{
		auto fileSize = VirtualFileSystem::get().size(path);

		if (fileSize < sizeof(SkeletonAssetData::FileHeader))
		{
			assetData->setAssetStatus(AssetStatus::ERROR);
			Log::err("SkeletonAssetHandler: Skeleton asset data file \"%s\" has a wrong format! (Too small to contain header data)", path);
			return false;
		}

		eastl::vector<char> fileData(fileSize);

		bool success = false;

		if (VirtualFileSystem::get().readFile(path, fileSize, fileData.data(), true))
		{
			const char *data = fileData.data();

			// we checked earlier that the header actually fits inside the file
			SkeletonAssetData::FileHeader header = *reinterpret_cast<const SkeletonAssetData::FileHeader *>(data);

			// check the magic number
			SkeletonAssetData::FileHeader defaultHeader{};
			if (memcmp(header.m_magicNumber, defaultHeader.m_magicNumber, sizeof(defaultHeader.m_magicNumber)) != 0)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("SkeletonAssetHandler: Skeleton asset data file \"%s\" has a wrong format! (Magic number does not match)", path);
				return false;
			}

			// check the version
			if (header.m_version != SkeletonAssetData::Version::LATEST)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("SkeletonAssetHandler: Skeleton asset data file \"%s\" has unsupported version \"%u\"!", path, (unsigned)header.m_version);
				return false;
			}

			// check the file size
			if (header.m_fileSize > fileSize)
			{
				assetData->setAssetStatus(AssetStatus::ERROR);
				Log::err("SkeletonAssetHandler: File size (%u) specified in header of skeleton asset data file \"%s\" is greater than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
				return false;
			}

			if (header.m_fileSize < fileSize)
			{
				Log::warn("SkeletonAssetHandler: File size (%u) specified in header of skeleton asset data file \"%s\" is less than actual file size (%u)!", path, (unsigned)header.m_fileSize, (unsigned)fileSize);
			}

			data += sizeof(header);
			size_t curFileOffset = 0;

			SkeletonCreateInfo skeletonCreateInfo{};
			{
				static_assert(sizeof(glm::mat4) == (sizeof(float) * 16));

				skeletonCreateInfo.m_jointCount = header.m_jointCount;

				char *memory = nullptr;

				size_t parentIndicesDstOffset = 0;
				size_t namesDstOffset = 0;
				size_t rawCopySize = 0;

				size_t memorySize = 0;

				// matrices
				memorySize += header.m_jointCount * sizeof(glm::mat4);

				// parent indices
				memorySize = util::alignPow2Up(memorySize, alignof(uint32_t));
				parentIndicesDstOffset = memorySize;
				memorySize += header.m_jointCount * sizeof(uint32_t);

				// we cannot copy the strings from the file directly, so we only copy up to this point
				// and handle the strings manually
				rawCopySize = memorySize;

				// names
				memorySize = util::alignPow2Up(memorySize, alignof(StringID));
				namesDstOffset = memorySize;
				memorySize += header.m_jointCount * sizeof(StringID);

				// allocate memory
				memory = new char[memorySize];
				assert(memory);

				assert((curFileOffset + rawCopySize) < fileSize);

				// copy data to our allocation
				memcpy(memory, data + curFileOffset, rawCopySize);

				// set up pointers
				skeletonCreateInfo.m_memory = memory;
				skeletonCreateInfo.m_invBindPoseMatrices = reinterpret_cast<const glm::mat4 *>(skeletonCreateInfo.m_memory);
				skeletonCreateInfo.m_parentIndices = reinterpret_cast<const uint32_t *>(skeletonCreateInfo.m_memory + parentIndicesDstOffset);
				skeletonCreateInfo.m_jointNames = reinterpret_cast<const StringID *>(skeletonCreateInfo.m_memory + namesDstOffset);


				// handle strings
				curFileOffset += rawCopySize;
				for (size_t i = 0; i < header.m_jointCount; ++i)
				{
					assert(curFileOffset < fileSize);

					size_t strLen = 0;
					while (data[curFileOffset + strLen] != '\0')
					{
						assert(curFileOffset + strLen < fileSize);
						++strLen;
					}

					assert(strLen);

					new (memory + namesDstOffset + i * sizeof(StringID)) StringID(data + curFileOffset);

					curFileOffset += strLen + 1; // dont forget the null terminator
				}
			}

			static_cast<SkeletonAssetData *>(assetData)->m_skeleton = Skeleton(skeletonCreateInfo);
			
			success = true;
		}
		
		if (!success)
		{
			Log::warn("SkeletonAssetHandler: Failed to load skeleton asset!");
			assetData->setAssetStatus(AssetStatus::ERROR);
			return false;
		}
	}

	assetData->setAssetStatus(AssetStatus::READY);

	return true;
}

void SkeletonAssetHandler::destroyAsset(const AssetID &assetID, const AssetType &assetType, AssetData *assetData) noexcept
{
	assert(assetData);
	if (assetData->getAssetType() != SkeletonAssetData::k_assetType)
	{
		Log::warn("SkeletonAssetHandler: Tried to call destroyAsset() on a non-skeleton asset!");
		return;
	}

	delete assetData;
}
