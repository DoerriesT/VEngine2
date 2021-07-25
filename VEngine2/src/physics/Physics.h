#pragma once
#include "Handles.h"
#include "utility/HandleManager.h"

class ECS;

// physx forward decls
namespace physx
{
	class PxFoundation;
	class PxPvd;
	class PxPhysics;
	class PxScene;
	class PxCpuDispatcher;
	class PxCooking;
	class PxMaterial;
}

struct PhysicsMaterialCreateInfo
{
	float m_staticFrictionCoeff = 0.5f;
	float m_dynamicFrictionCoeff = 0.5f;
	float m_restitutionCoeff = 0.6f;
};

class Physics
{
public:
	explicit Physics(ECS *ecs) noexcept;
	~Physics() noexcept;
	void update(float deltaTime) noexcept;
	void createMaterials(uint32_t count, const PhysicsMaterialCreateInfo *materials, PhysicsMaterialHandle *handles) noexcept;
	void updateMaterials(uint32_t count, const PhysicsMaterialCreateInfo *materials, PhysicsMaterialHandle *handles) noexcept;
	void destroyMaterials(uint32_t count, PhysicsMaterialHandle *handles) noexcept;

private:
	ECS *m_ecs = nullptr;
	physx::PxFoundation *m_pxFoundation = nullptr;
	physx::PxPvd *m_pxPvd = nullptr;
	physx::PxPhysics *m_pxPhysics = nullptr;
	physx::PxScene *m_pxScene = nullptr;
	physx::PxCpuDispatcher *m_pxCpuDispatcher = nullptr;
	physx::PxCooking *m_pxCooking = nullptr;
	HandleManager m_materialHandleManager;
	eastl::vector<physx::PxMaterial *> m_materials;
	float m_timeAccumulator = 0.0f;
};