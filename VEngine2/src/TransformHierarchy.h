#pragma once
#include "utility/Transform.h"
#include "ecs/ECSCommon.h"

class ECS;
struct TransformComponent;

namespace TransformHierarchy
{
	bool attach(ECS *ecs, EntityID childEntity, EntityID parentEntity, bool keepLocalTransform, TransformComponent *childTransformComponent = nullptr) noexcept;
	void detach(ECS *ecs, EntityID entity, bool orphanChildren, TransformComponent *transformComponent = nullptr) noexcept;
	void setLocalTransform(ECS *ecs, EntityID entity, const Transform &transform, TransformComponent *transformComponent = nullptr) noexcept;
	void setGlobalTransform(ECS *ecs, EntityID entity, const Transform &transform, TransformComponent *transformComponent = nullptr) noexcept;
	uint32_t setMobility(ECS *ecs, EntityID entity, Mobility mobility, TransformComponent *transformComponent = nullptr) noexcept;
	EntityID getNextSibling(ECS *ecs, EntityID entity) noexcept;
}