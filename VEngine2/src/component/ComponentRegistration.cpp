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
#include "OutlineComponent.h"
#include "ParticipatingMediumComponent.h"
#include "ReflectionProbeComponent.h"
#include "IrradianceVolumeComponent.h"
#include "RawInputStateComponent.h"
#include "InputStateComponent.h"
#include "ecs/ECS.h"
#include "ecs/ECSComponentInfoTable.h"

template<typename T>
static void registerComponent()
{
	ECS::registerComponent<T>();
	ECSComponentInfoTable::registerType<T>();
}

void ComponentRegistration::registerAllComponents() noexcept
{
	// regular components
	registerComponent<TransformComponent>();
	registerComponent<CameraComponent>();
	registerComponent<LightComponent>();
	registerComponent<MeshComponent>();
	registerComponent<PhysicsComponent>();
	registerComponent<CharacterControllerComponent>();
	registerComponent<CharacterMovementComponent>();
	registerComponent<SkinnedMeshComponent>();
	registerComponent<ScriptComponent>();
	registerComponent<OutlineComponent>();
	registerComponent<EditorOutlineComponent>();
	registerComponent<ParticipatingMediumComponent>();
	registerComponent<ReflectionProbeComponent>();
	registerComponent<IrradianceVolumeComponent>();

	// singleton components
	ECS::registerSingletonComponent<RawInputStateComponent>();
	ECS::registerSingletonComponent<InputStateComponent>();
}
