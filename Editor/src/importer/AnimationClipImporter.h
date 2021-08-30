#pragma once

struct LoadedAnimationClip;

namespace AnimationClipImporter
{
	bool importAnimationClips(size_t count, const LoadedAnimationClip *anims, const char *baseDstPath, const char *sourcePath) noexcept;
}