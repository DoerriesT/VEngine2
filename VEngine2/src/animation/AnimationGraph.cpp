#include "AnimationGraph.h"
#include "AnimationSystem.h"

AnimationGraphContext::AnimationGraphContext(AnimationSystem *animationSystem) noexcept
	:m_animationSystem(animationSystem)
{
}

const AnimationClip *AnimationGraphContext::getAnimationClip(AnimationClipHandle handle) const noexcept
{
	size_t idx = handle - 1;
	if (idx < m_animationSystem->m_animationClips.size())
	{
		return m_animationSystem->m_animationClips[idx];
	}
	else
	{
		return nullptr;
	}
}
