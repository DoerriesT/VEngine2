#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"
#include "ecs/ECSCommon.h"

struct lua_State;
struct TransformComponent;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

enum class PhysicsMobility : uint32_t
{
	STATIC = 0,
	DYNAMIC = 1,
	KINEMATIC = 2
};

enum class PhysicsShapeType : uint32_t
{
	SPHERE,
	PLANE,
	CONVEX_MESH,
	TRIANGLE_MESH
};

struct PhysicsComponent
{
	PhysicsMobility m_mobility = PhysicsMobility::STATIC;
	PhysicsShapeType m_physicsShapeType = PhysicsShapeType::SPHERE;
	Asset<MeshAsset> m_physicsMesh;
	float m_sphereRadius = 1.0f;
	float m_planeNx = 0.0f;
	float m_planeNy = 1.0f;
	float m_planeNz = 0.0f;
	float m_planeDistance = 0.0f;
	float m_linearDamping = 0.05f;
	float m_angularDamping = 0.05f;
	float m_density = 10.0f;
	float m_initialVelocityX = 0.0f;
	float m_initialVelocityY = 0.0f;
	float m_initialVelocityZ = 0.0f;
	//PhysicsMaterialHandle m_materialHandle = {};
	void *m_internalPhysicsActorHandle = nullptr;

	PhysicsComponent() = default;
	PhysicsComponent(const PhysicsComponent & other) noexcept;
	PhysicsComponent(PhysicsComponent && other) noexcept;
	PhysicsComponent &operator=(const PhysicsComponent & other) noexcept;
	PhysicsComponent &operator=(PhysicsComponent && other) noexcept;
	~PhysicsComponent() noexcept;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "PhysicsComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Physics Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};