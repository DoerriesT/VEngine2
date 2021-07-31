#include "ComponentRegistration.h"
#include "TransformComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "MeshComponent.h"
#include "PhysicsComponent.h"
#include "CharacterControllerComponent.h"
#include "PlayerMovementComponent.h"
#include "InputStateComponent.h"
#include "ecs/ECS.h"
#include "ecs/ECSTypeIDTranslator.h"
#include "reflection/Reflection.h"

template<typename T>
static void registerComponent(ECS *ecs)
{
	T::reflect(g_Reflection);
	ecs->registerComponent<T>();
	ECSTypeIDTranslator::registerType<T>();
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
	registerComponent<PlayerMovementComponent>(ecs);

	// singleton components
	ecs->registerSingletonComponent<RawInputStateComponent>(RawInputStateComponent{});
}
