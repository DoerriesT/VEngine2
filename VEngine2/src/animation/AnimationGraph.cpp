#include "AnimationGraph.h"
#include "AnimationSystem.h"

AnimationGraphContext::AnimationGraphContext(AnimationSystem *animationSystem, ECS *ecs, EntityID entity) noexcept
	:m_animationSystem(animationSystem),
	m_ecs(ecs),
	m_entity(entity)
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

ECS *AnimationGraphContext::getECS() noexcept
{
	return m_ecs;
}

EntityID AnimationGraphContext::getEntityID() const noexcept
{
	return m_entity;
}
