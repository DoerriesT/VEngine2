#pragma once
#include "Handles.h"
#include "ecs/ECSCommon.h"

struct JointPose;
class AnimationClip;
class AnimationSystem;
class ECS;

class AnimationGraphContext
{
public:
	explicit AnimationGraphContext(AnimationSystem *animationSystem, ECS *ecs, EntityID) noexcept;
	const AnimationClip *getAnimationClip(AnimationClipHandle handle) const noexcept;
	EntityID getEntityID() const noexcept;
	ECS *getECS() noexcept;

private:
	AnimationSystem *m_animationSystem;
	ECS *m_ecs;
	EntityID m_entity;
};

class AnimationGraph
{
public:
	virtual void preExecute(AnimationGraphContext *context, float deltaTime) noexcept = 0;
	virtual void execute(AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept = 0;
	virtual void postExecute(AnimationGraphContext *context, float deltaTime) noexcept = 0;
	virtual bool isActive() const noexcept = 0;
};