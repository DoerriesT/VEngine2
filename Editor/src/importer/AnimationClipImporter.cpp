#include "AnimationClipImporter.h"
#include <asset/AssetManager.h>
#include <asset/AnimationClipAsset.h>
#include <filesystem/VirtualFileSystem.h>
#include <Log.h>
#include "loader/LoadedModel.h"
#include <EASTL/string.h>

bool AnimationClipImporter::importAnimationClips(size_t count, const LoadedAnimationClip *anims, const char *baseDstPath, const char *sourcePath) noexcept
{
	auto *assetMgr = AssetManager::get();
	auto &vfs = VirtualFileSystem::get();

	for (size_t i = 0; i < count; ++i)
	{
		const auto &animClip = anims[i];

		eastl::string cleanedName = animClip.m_name;
		cleanedName.erase(eastl::remove_if(cleanedName.begin(), cleanedName.end(), [](auto c)
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

		eastl::string dstPath = count > 1 ? (eastl::string(baseDstPath) + "_" + cleanedName + ".anim") : eastl::string(baseDstPath) + ".anim";

		assetMgr->createAsset(AnimationClipAsset::k_assetType, dstPath.c_str(), sourcePath);

		if (FileHandle fh = vfs.open(dstPath.c_str(), FileMode::WRITE, true))
		{
			AnimationClipAsset::FileHeader header{};
			header.m_fileSize = sizeof(header);
			header.m_jointCount = (uint32_t)animClip.m_jointAnimations.size();
			header.m_duration = animClip.m_duration;

			// compute file size
			{
				header.m_fileSize += header.m_jointCount * sizeof(uint32_t) * 6; // per joint info
				for (const auto &jointClip : animClip.m_jointAnimations)
				{
					header.m_fileSize += (uint32_t)jointClip.m_translationChannel.m_timeKeys.size() * sizeof(uint32_t);
					header.m_fileSize += (uint32_t)jointClip.m_rotationChannel.m_timeKeys.size() * sizeof(uint32_t);
					header.m_fileSize += (uint32_t)jointClip.m_scaleChannel.m_timeKeys.size() * sizeof(uint32_t);
					header.m_fileSize += (uint32_t)jointClip.m_translationChannel.m_translations.size() * sizeof(glm::vec3);
					header.m_fileSize += (uint32_t)jointClip.m_rotationChannel.m_rotations.size() * sizeof(glm::quat);
					header.m_fileSize += (uint32_t)jointClip.m_scaleChannel.m_scales.size() * sizeof(float);
				}
			}

			vfs.write(fh, sizeof(header), &header);

			uint32_t curTranslationArrayOffset = 0;
			uint32_t curRotationArrayOffset = 0;
			uint32_t curScaleArrayOffset = 0;

			// write joint clip info
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				uint32_t translationCount = (uint32_t)jointClip.m_translationChannel.m_timeKeys.size();
				uint32_t rotationCount = (uint32_t)jointClip.m_rotationChannel.m_timeKeys.size();
				uint32_t scaleCount = (uint32_t)jointClip.m_scaleChannel.m_timeKeys.size();

				vfs.write(fh, 4, &translationCount);
				vfs.write(fh, 4, &rotationCount);
				vfs.write(fh, 4, &scaleCount);
				vfs.write(fh, 4, &curTranslationArrayOffset);
				vfs.write(fh, 4, &curRotationArrayOffset);
				vfs.write(fh, 4, &curScaleArrayOffset);

				curTranslationArrayOffset += (uint32_t)jointClip.m_translationChannel.m_timeKeys.size();
				curRotationArrayOffset += (uint32_t)jointClip.m_rotationChannel.m_timeKeys.size();
				curScaleArrayOffset += (uint32_t)jointClip.m_scaleChannel.m_timeKeys.size();
			}

			// write translation timekeys
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				for (auto t : jointClip.m_translationChannel.m_timeKeys)
				{
					vfs.write(fh, 4, &t);
				}
			}

			// write rotation timekeys
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				for (auto t : jointClip.m_rotationChannel.m_timeKeys)
				{
					vfs.write(fh, 4, &t);
				}
			}

			// write scale timekeys
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				for (auto t : jointClip.m_scaleChannel.m_timeKeys)
				{
					vfs.write(fh, 4, &t);
				}
			}

			// write translations
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				for (const auto &t : jointClip.m_translationChannel.m_translations)
				{
					vfs.write(fh, sizeof(float) * 3, &t.x);
				}
			}

			// write rotations
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				for (const auto &r : jointClip.m_rotationChannel.m_rotations)
				{
					vfs.write(fh, sizeof(float) * 4, &r.x);
				}
			}

			// write scales
			for (const auto &jointClip : animClip.m_jointAnimations)
			{
				for (const auto &s : jointClip.m_scaleChannel.m_scales)
				{
					vfs.write(fh, sizeof(float), &s);
				}
			}

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
