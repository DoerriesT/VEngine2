#include "SkeletonImporter.h"
#include <asset/AssetManager.h>
#include <asset/SkeletonAsset.h>
#include <filesystem/VirtualFileSystem.h>
#include <Log.h>
#include <EASTL/string.h>
#include "loader/LoadedModel.h"

bool SkeletonImporter::importSkeletons(size_t count, const LoadedSkeleton *skeletons, const char *baseDstPath, const char *sourcePath) noexcept
{
	auto *assetMgr = AssetManager::get();
	auto &vfs = VirtualFileSystem::get();

	// store skeletons
	for (size_t i = 0; i < count; ++i)
	{
		eastl::string dstPath = count > 1 ? (eastl::string(baseDstPath) + "_skeleton" + eastl::to_string(i) + ".skel") : (eastl::string(baseDstPath) + ".skel");

		assetMgr->createAsset(SkeletonAssetData::k_assetType, dstPath.c_str(), sourcePath);

		if (FileHandle fh = vfs.open(dstPath.c_str(), FileMode::WRITE, true))
		{
			const auto &skele = skeletons[i];

			// write joint count
			const uint32_t jointCount = static_cast<uint32_t>(skele.m_joints.size());
			vfs.write(fh, 4, &jointCount);

			// write joints
			for (size_t j = 0; j < jointCount; ++j)
			{
				vfs.write(fh, sizeof(float) * 16, &skele.m_joints[j].m_invBindPose[0][0]);
			}

			// write parent indices
			for (size_t j = 0; j < jointCount; ++j)
			{
				vfs.write(fh, sizeof(LoadedSkeleton::Joint::m_parentIdx), &skele.m_joints[j].m_parentIdx);
			}

			// write joint names
			for (size_t j = 0; j < jointCount; ++j)
			{
				const char *str = skele.m_joints[j].m_name.c_str();
				vfs.write(fh, skele.m_joints[j].m_name.length() + 1, str);
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
