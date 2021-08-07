#pragma once
#include "Handles.h"

struct JointPose;
class AnimationClip;
class AnimationSystem;

class AnimationGraphContext
{
public:
	explicit AnimationGraphContext(AnimationSystem *animationSystem) noexcept;
	const AnimationClip *getAnimationClip(AnimationClipHandle handle) const noexcept;

private:
	AnimationSystem *m_animationSystem;
};

class AnimationGraph
{
public:
	virtual void preExecute(const AnimationGraphContext *context, float deltaTime) noexcept = 0;
	virtual void execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept = 0;
	virtual void postExecute(const AnimationGraphContext *context, float deltaTime) noexcept = 0;
	virtual bool isActive() const noexcept = 0;
};