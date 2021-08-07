#pragma once
#include <EASTL/vector.h>
#include "Handles.h"
#include "utility/HandleManager.h"

class ECS;

class Skeleton;
class AnimationClip;

class AnimationSystem
{
	friend class AnimationGraphContext;
public:
	explicit AnimationSystem(ECS *ecs) noexcept;
	~AnimationSystem() noexcept;
	void update(float deltaTime) noexcept;
	AnimationSkeletonHandle createSkeleton(size_t fileSize, const char *data) noexcept;
	void destroySkeleton(AnimationSkeletonHandle handle) noexcept;
	AnimationClipHandle createAnimationClip(size_t fileSize, const char *data) noexcept;
	void destroyAnimationClip(AnimationClipHandle handle) noexcept;

private:
	ECS *m_ecs = nullptr;
	HandleManager m_skeletonHandleManager;
	HandleManager m_animationClipHandleManager;
	eastl::vector<Skeleton *> m_skeletons;
	eastl::vector<AnimationClip *> m_animationClips;
};