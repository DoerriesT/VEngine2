#pragma once

struct LoadedSkeleton;

namespace SkeletonImporter
{
	bool importSkeletons(size_t count, const LoadedSkeleton *skeletons, const char *baseDstPath, const char *sourcePath) noexcept;
}