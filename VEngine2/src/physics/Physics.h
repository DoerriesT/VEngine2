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
	class PxConvexMesh;
	class PxTriangleMesh;
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
	PhysicsConvexMeshHandle createConvexMesh(uint32_t size, const char *data) noexcept;
	void destroyConvexMesh(PhysicsConvexMeshHandle handle) noexcept;
	PhysicsTriangleMeshHandle createTriangleMesh(uint32_t size, const char *data) noexcept;
	void destroyTriangleMesh(PhysicsTriangleMeshHandle handle) noexcept;
	bool cookConvexMesh(uint32_t vertexCount, const float *vertices, uint32_t *resultBufferSize, char **resultBuffer) noexcept;
	bool cookTriangleMesh(uint32_t indexCount, const uint32_t *indices, uint32_t vertexCount, const float *vertices, uint32_t *resultBufferSize, char **resultBuffer) noexcept;

private:
	ECS *m_ecs = nullptr;
	physx::PxFoundation *m_pxFoundation = nullptr;
	physx::PxPvd *m_pxPvd = nullptr;
	physx::PxPhysics *m_pxPhysics = nullptr;
	physx::PxScene *m_pxScene = nullptr;
	physx::PxCpuDispatcher *m_pxCpuDispatcher = nullptr;
	physx::PxCooking *m_pxCooking = nullptr;
	HandleManager m_materialHandleManager;
	HandleManager m_convexMeshHandleManager;
	HandleManager m_triangleMeshHandleManager;
	eastl::vector<physx::PxMaterial *> m_materials;
	eastl::vector<physx::PxConvexMesh *> m_convexMeshes;
	eastl::vector<physx::PxTriangleMesh *> m_triangleMeshes;
	float m_timeAccumulator = 0.0f;
};