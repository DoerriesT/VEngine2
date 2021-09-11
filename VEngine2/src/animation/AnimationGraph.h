#pragma once
#include "Handles.h"
#include "ecs/ECSCommon.h"

struct JointPose;
class ECS;

class AnimationGraph
{
public:
	virtual void preExecute(ECS *ecs, EntityID entity, float deltaTime) noexcept = 0;
	virtual void execute(ECS *ecs, EntityID entity, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept = 0;
	virtual void postExecute(ECS *ecs, EntityID entity, float deltaTime) noexcept = 0;
	virtual bool isActive() const noexcept = 0;
};