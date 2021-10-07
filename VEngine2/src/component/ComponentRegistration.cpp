#include "ComponentRegistration.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MeshComponent.h"
#include "PhysicsComponent.h"
#include "CharacterControllerComponent.h"
#include "CharacterMovementComponent.h"
#include "SkinnedMeshComponent.h"
#include "ScriptComponent.h"
#include "RawInputStateComponent.h"
#include "InputStateComponent.h"
#include "ecs/ECS.h"
#include "ecs/ECSComponentInfoTable.h"

template<typename T>
static void registerComponent(ECS *ecs)
{
	ecs->registerComponent<T>();
	ECSComponentInfoTable::registerType<T>();
}

void ComponentRegistration::registerAllComponents(ECS *ecs) noexcept
{
	// regular components
	registerComponent<TransformComponent>(ecs);
	registerComponent<CameraComponent>(ecs);
	registerComponent<LightComponent>(ecs);
	registerComponent<MeshComponent>(ecs);
	registerComponent<PhysicsComponent>(ecs);
	registerComponent<CharacterControllerComponent>(ecs);
	registerComponent<CharacterMovementComponent>(ecs);
	registerComponent<SkinnedMeshComponent>(ecs);
	registerComponent<ScriptComponent>(ecs);

	// singleton components
	ecs->registerSingletonComponent<RawInputStateComponent>(RawInputStateComponent{});
	ecs->registerSingletonComponent<InputStateComponent>(InputStateComponent{});
}
