#include "Physics.h"
#include "Log.h"
#include <assert.h>
#include <PxPhysicsAPI.h>
#include "ecs/ECS.h"
#include "component/PhysicsComponent.h"
#include "component/TransformComponent.h"
#include "component/CharacterControllerComponent.h"
#include "profiling/Profiling.h"
#include "job/JobSystem.h"
#include "job/ParallelFor.h"

#define PX_RELEASE(x) if(x) { x->release(); x = NULL;	}

using namespace physx;

namespace
{
	class UserAllocatorCallback : public PxAllocatorCallback
	{
	public:
		~UserAllocatorCallback() {}
		void *allocate(size_t size, const char *typeName, const char *filename, int line) override
		{
			return malloc(size);
		}

		void deallocate(void *ptr) override
		{
			free(ptr);
		}
	};

	class UserErrorCallback : public PxErrorCallback
	{
	public:
		virtual void reportError(PxErrorCode::Enum code, const char *message, const char *file, int line)
		{
			switch (code)
			{
			case physx::PxErrorCode::eNO_ERROR:
				break;
			case physx::PxErrorCode::eDEBUG_INFO:
				Log::info(message);
				break;
			case physx::PxErrorCode::eDEBUG_WARNING:
				Log::warn(message);
				break;
			case physx::PxErrorCode::eINVALID_PARAMETER:
				Log::err(message);
				break;
			case physx::PxErrorCode::eINVALID_OPERATION:
				Log::err(message);
				break;
			case physx::PxErrorCode::eOUT_OF_MEMORY:
				Log::err(message);
				break;
			case physx::PxErrorCode::eINTERNAL_ERROR:
				Log::err(message);
				break;
			case physx::PxErrorCode::eABORT:
				Log::err(message);
				break;
			case physx::PxErrorCode::ePERF_WARNING:
				Log::warn(message);
				break;
			case physx::PxErrorCode::eMASK_ALL:
				Log::err(message);
				break;
			default:
				Log::err(message);
				break;
			}
		}
	};

	class UserCpuDispatcher : public PxCpuDispatcher
	{
		void submitTask(PxBaseTask &task) noexcept override
		{
			job::Job j([](void *arg)
				{
					PxBaseTask *task = reinterpret_cast<PxBaseTask *>(arg);
					task->run();
					task->release();
				}, &task);

			job::run(1, &j, nullptr);
		}

		uint32_t getWorkerCount() const noexcept override
		{
			return static_cast<uint32_t>(job::getThreadCount());
		}
	};
}

static UserAllocatorCallback s_physxAllocatorCallback;
static UserErrorCallback s_physxErrorCallback;


Physics::Physics(ECS *ecs) noexcept
	:m_ecs(ecs)
{
	m_pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_physxAllocatorCallback, s_physxErrorCallback);
	assert(m_pxFoundation);

	bool recordMemoryAllocations = true;

	m_pxPvd = PxCreatePvd(*m_pxFoundation);
	PxPvdTransport *transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
	m_pxPvd->connect(*transport, PxPvdInstrumentationFlag::eALL);


	m_pxPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pxFoundation, PxTolerancesScale(), recordMemoryAllocations, m_pxPvd);
	assert(m_pxPhysics);

	m_pxCooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_pxFoundation, PxCookingParams(m_pxPhysics->getTolerancesScale()));
	assert(m_pxCooking);


	PxSceneDesc sceneDesc(m_pxPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	m_pxCpuDispatcher = new UserCpuDispatcher();
	sceneDesc.cpuDispatcher = m_pxCpuDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	m_pxScene = m_pxPhysics->createScene(sceneDesc);

	PxPvdSceneClient *pvdClient = m_pxScene->getScenePvdClient();
	if (pvdClient)
	{
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
		pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
	}

	m_controllerManager = PxCreateControllerManager(*m_pxScene, true);
	assert(m_controllerManager);

	m_materials.resize(16);
	m_convexMeshes.resize(16);
	m_triangleMeshes.resize(16);
}

Physics::~Physics() noexcept
{
	PX_RELEASE(m_controllerManager);
	PX_RELEASE(m_pxCooking);
	PX_RELEASE(m_pxPhysics);
	PX_RELEASE(m_pxPvd);
	PX_RELEASE(m_pxFoundation);

	delete m_pxCpuDispatcher;
	m_pxCpuDispatcher = nullptr;
}

void Physics::update(float deltaTime) noexcept
{
	PROFILING_ZONE_SCOPED;

	// initialize new entities and apply kinematics
	m_ecs->iterate<TransformComponent, PhysicsComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, PhysicsComponent *physicsC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &pc = physicsC[i];
				auto &tc = transC[i];

				// initialize actor
				if (!pc.m_internalPhysicsActorHandle)
				{
					//const bool validMaterialHandle = pc.m_materialHandle != 0 && pc.m_materialHandle <= m_materials.size();
					//if (!validMaterialHandle)
					//{
					//	// TODO: some error message
					//	continue;
					//}

					if (pc.m_physicsShapeType == PhysicsShapeType::CONVEX_MESH || pc.m_physicsShapeType == PhysicsShapeType::TRIANGLE_MESH)
					{
						if (pc.m_physicsMesh.isLoaded())
						{
							continue;
						}
						if (pc.m_physicsShapeType == PhysicsShapeType::CONVEX_MESH && pc.m_physicsMesh->getPhysicsConvexMeshhandle() == NULL_PHYSICS_CONVEX_MESH_HANDLE)
						{
							continue;
						}
						if (pc.m_physicsShapeType == PhysicsShapeType::TRIANGLE_MESH && pc.m_physicsMesh->getPhysicsTriangleMeshhandle() == NULL_PHYSICS_TRIANGLE_MESH_HANDLE)
						{
							continue;
						}
					}

					// fetch material
					PxMaterial *mat = m_pxPhysics->createMaterial(0.5f, 0.5f, 0.5f); //m_materials[pc.m_materialHandle - 1];

					// create transform
					PxTransform pxTransform{};
					pxTransform.p.x = tc.m_transform.m_translation.x;
					pxTransform.p.y = tc.m_transform.m_translation.y;
					pxTransform.p.z = tc.m_transform.m_translation.z;
					pxTransform.q.x = tc.m_transform.m_rotation.x;
					pxTransform.q.y = tc.m_transform.m_rotation.y;
					pxTransform.q.z = tc.m_transform.m_rotation.z;
					pxTransform.q.w = tc.m_transform.m_rotation.w;


					// create actor
					PxRigidActor *actor = nullptr;

					if (pc.m_mobility == PhysicsMobility::STATIC)
					{
						switch (pc.m_physicsShapeType)
						{
						case PhysicsShapeType::SPHERE:
							actor = PxCreateStatic(*m_pxPhysics, pxTransform, PxSphereGeometry(pc.m_sphereRadius), *mat);
							break;
						case PhysicsShapeType::PLANE:
							actor = PxCreatePlane(*m_pxPhysics, PxPlane(pc.m_planeNx, pc.m_planeNy, pc.m_planeNz, pc.m_planeDistance), *mat);
							break;
						case PhysicsShapeType::CONVEX_MESH:
							actor = PxCreateStatic(*m_pxPhysics, pxTransform, PxConvexMeshGeometry(m_convexMeshes[pc.m_physicsMesh->getPhysicsConvexMeshhandle() - 1]), *mat);
							break;
						case PhysicsShapeType::TRIANGLE_MESH:
							actor = PxCreateStatic(*m_pxPhysics, pxTransform, PxTriangleMeshGeometry(m_triangleMeshes[pc.m_physicsMesh->getPhysicsTriangleMeshhandle() - 1]), *mat);
							break;
						default:
							assert(false);
							break;
						}
					}
					else
					{
						PxRigidDynamic *dynamic = nullptr;
						switch (pc.m_physicsShapeType)
						{
						case PhysicsShapeType::SPHERE:
							dynamic = PxCreateDynamic(*m_pxPhysics, pxTransform, PxSphereGeometry(pc.m_sphereRadius), *mat, pc.m_density);
							break;
						case PhysicsShapeType::CONVEX_MESH:
							dynamic = PxCreateDynamic(*m_pxPhysics, pxTransform, PxConvexMeshGeometry(m_convexMeshes[pc.m_physicsMesh->getPhysicsConvexMeshhandle() - 1]), *mat, pc.m_density);
							break;
						default:
							assert(false);
							break;
						}

						if (pc.m_mobility == PhysicsMobility::KINEMATIC)
						{
							dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
						}
						else
						{
							dynamic->setLinearDamping(pc.m_linearDamping);
							dynamic->setAngularDamping(pc.m_angularDamping);
							dynamic->setLinearVelocity(PxVec3(pc.m_initialVelocityX, pc.m_initialVelocityY, pc.m_initialVelocityZ));
						}

						actor = dynamic;
					}

					actor->userData = (void *)entities[i];

					m_pxScene->addActor(*actor);

					pc.m_internalPhysicsActorHandle = actor;
				}

				// set kinematic transform
				if (pc.m_mobility == PhysicsMobility::KINEMATIC)
				{
					// create transform
					PxTransform pxTransform{};
					pxTransform.p.x = tc.m_transform.m_translation.x;
					pxTransform.p.y = tc.m_transform.m_translation.y;
					pxTransform.p.z = tc.m_transform.m_translation.z;
					pxTransform.q.x = tc.m_transform.m_rotation.x;
					pxTransform.q.y = tc.m_transform.m_rotation.y;
					pxTransform.q.z = tc.m_transform.m_rotation.z;
					pxTransform.q.w = tc.m_transform.m_rotation.w;

					((PxRigidDynamic *)pc.m_internalPhysicsActorHandle)->setKinematicTarget(pxTransform);
				}
			}
		});

	// character controller
	m_ecs->iterate<TransformComponent, CharacterControllerComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, CharacterControllerComponent *ccC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &cc = ccC[i];
				auto &tc = transC[i];

				const float centerToTranslationCompHeight = cc.m_translationHeightOffset - cc.m_capsuleHeight * 0.5f;
				const float adjustedHeight = cc.m_capsuleHeight - 2.0f * cc.m_contactOffset;
				const float adjustedRadius = cc.m_capsuleRadius - cc.m_contactOffset;

				if (!cc.m_internalControllerHandle)
				{
					PxCapsuleControllerDesc  controllerDesc{};
					controllerDesc.radius = adjustedRadius;
					controllerDesc.height = adjustedHeight - 2.0f * adjustedRadius;
					controllerDesc.position.set(tc.m_transform.m_translation.x, tc.m_transform.m_translation.y - centerToTranslationCompHeight, tc.m_transform.m_translation.z);
					controllerDesc.slopeLimit = cosf(cc.m_slopeLimit);
					controllerDesc.contactOffset = cc.m_contactOffset;
					controllerDesc.stepOffset = cc.m_stepOffset;
					controllerDesc.density = cc.m_density;
					controllerDesc.material = m_pxPhysics->createMaterial(0.5f, 0.5f, 0.5f);

					PxController *controller = m_controllerManager->createController(controllerDesc);
					controller->getActor()->userData = (void *)entities[i];
					cc.m_internalControllerHandle = controller;
				}

				PxController *controller = (PxController *)cc.m_internalControllerHandle;

				controller->resize(adjustedHeight - 2.0f * adjustedRadius);

				cc.m_collisionFlags = CharacterControllerCollisionFlags::NONE;

				if (cc.m_active)
				{
					auto collisionFlags = controller->move(PxVec3(cc.m_movementDeltaX, cc.m_movementDeltaY, cc.m_movementDeltaZ), 0.00001f, deltaTime, PxControllerFilters());
					cc.m_collisionFlags |= collisionFlags.isSet(PxControllerCollisionFlag::eCOLLISION_SIDES) ? CharacterControllerCollisionFlags::SIDES : CharacterControllerCollisionFlags::NONE;
					cc.m_collisionFlags |= collisionFlags.isSet(PxControllerCollisionFlag::eCOLLISION_UP) ? CharacterControllerCollisionFlags::UP : CharacterControllerCollisionFlags::NONE;
					cc.m_collisionFlags |= collisionFlags.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN) ? CharacterControllerCollisionFlags::DOWN : CharacterControllerCollisionFlags::NONE;
				
					auto centerPos = controller->getPosition();
					tc.m_transform.m_translation.x = (float)centerPos.x;
					tc.m_transform.m_translation.y = (float)centerPos.y + centerToTranslationCompHeight;
					tc.m_transform.m_translation.z = (float)centerPos.z;
				}
				else
				{
					controller->setPosition(PxExtendedVec3(tc.m_transform.m_translation.x, tc.m_transform.m_translation.y - centerToTranslationCompHeight, tc.m_transform.m_translation.z));
				}
				
				// movement has been consumed
				cc.m_movementDeltaX = 0.0f;
				cc.m_movementDeltaY = 0.0f;
				cc.m_movementDeltaZ = 0.0f;
			}
		});

	// step the simulation
	m_pxScene->simulate(deltaTime);
	m_pxScene->fetchResults(true);
	//constexpr float k_stepSize = 1.0f / 60.0f;
	//m_timeAccumulator += deltaTime;
	//while (m_timeAccumulator >= k_stepSize)
	//{
	//	m_timeAccumulator -= k_stepSize;
	//	m_pxScene->simulate(k_stepSize);
	//	m_pxScene->fetchResults(true);
	//}


	// sync entities with physics state
	m_ecs->iterate<TransformComponent, PhysicsComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, PhysicsComponent *physicsC)
		{
			job::parallelFor(count, 1,
				[&](size_t startIdx, size_t endIdx)
				{
					for (size_t i = startIdx; i < endIdx; ++i)
					{
						auto &tc = transC[i];
						auto &pc = physicsC[i];

						if (pc.m_mobility == PhysicsMobility::DYNAMIC)
						{
							auto pxTc = ((PxRigidDynamic *)pc.m_internalPhysicsActorHandle)->getGlobalPose();
							tc.m_transform.m_translation.x = pxTc.p.x;
							tc.m_transform.m_translation.y = pxTc.p.y;
							tc.m_transform.m_translation.z = pxTc.p.z;
							tc.m_transform.m_rotation.x = pxTc.q.x;
							tc.m_transform.m_rotation.y = pxTc.q.y;
							tc.m_transform.m_rotation.z = pxTc.q.z;
							tc.m_transform.m_rotation.w = pxTc.q.w;
						}
					}
				});
		});
}

bool Physics::raycast(const float *origin, const float *dir, float maxT, RayCastResult *result) const noexcept
{
	*result = {};

	PxRaycastBuffer hit;
	if (m_pxScene->raycast(
		PxVec3(origin[0], origin[1], origin[2]),
		PxVec3(dir[0], dir[1], dir[2]),
		maxT,
		hit))
	{
		
		result->m_hit = true;
		result->m_rayT = hit.block.distance;
		memcpy(result->m_hitNormal, &hit.block.normal.x, sizeof(float) * 3);
		result->m_hitEntity = (EntityID)(hit.block.actor->userData);

		return true;
	}

	
	return false;
}

void Physics::createMaterials(uint32_t count, const PhysicsMaterialCreateInfo *materials, PhysicsMaterialHandle *handles) noexcept
{
	// LOCK_HOLDER(m_materialsMutex);
	for (size_t i = 0; i < count; ++i)
	{
		// allocate handle
		{
			// LOCK_HOLDER(m_handleManagerMutex);
			handles[i] = (PhysicsMaterialHandle)m_materialHandleManager.allocate();
		}

		if (!handles[i])
		{
			Log::err("Physics: Failed to allocate PhysicsMaterialHandle!");
			continue;
		}

		// store material
		{
			///LOCK_HOLDER(m_materialsMutex);
			if (handles[i] > m_materials.size())
			{
				m_materials.resize((size_t)(m_materials.size() * 1.5));
			}

			auto *mat = m_pxPhysics->createMaterial(materials[i].m_staticFrictionCoeff, materials[i].m_dynamicFrictionCoeff, materials[i].m_restitutionCoeff);
			m_materials[handles[i] - 1] = mat;
		}
	}
}

void Physics::updateMaterials(uint32_t count, const PhysicsMaterialCreateInfo *materials, PhysicsMaterialHandle *handles) noexcept
{
	//LOCK_HOLDER(m_materialsMutex);
	for (size_t i = 0; i < count; ++i)
	{
		const bool validHandle = handles[i] != 0 && handles[i] <= m_materials.size();
		if (!validHandle)
		{
			Log::warn("Physics: Tried to update an invalid PhysicsMaterialHandle!");
			return;
		}

		auto &mat = m_materials[handles[i] - 1];
		mat->setStaticFriction(materials[i].m_staticFrictionCoeff);
		mat->setDynamicFriction(materials[i].m_dynamicFrictionCoeff);
		mat->setRestitution(materials[i].m_restitutionCoeff);
	}
}

void Physics::destroyMaterials(uint32_t count, PhysicsMaterialHandle *handles) noexcept
{
	// LOCK_HOLDER(m_materialsMutex);
	for (size_t i = 0; i < count; ++i)
	{
		const bool validHandle = handles[i] != 0 && handles[i] <= m_materials.size();

		if (!validHandle)
		{
			continue;
		}

		PX_RELEASE(m_materials[handles[i] - 1]);

		{
			//LOCK_HOLDER(m_handleManagerMutex);
			m_materialHandleManager.free(handles[i]);
		}
	}
}

PhysicsConvexMeshHandle Physics::createConvexMesh(uint32_t size, const char *data) noexcept
{
	PhysicsConvexMeshHandle handle{};

	// LOCK_HOLDER(m_materialsMutex);

	// allocate handle
	{
		// LOCK_HOLDER(m_handleManagerMutex);
		handle = (PhysicsConvexMeshHandle)m_convexMeshHandleManager.allocate();
	}

	if (!handle)
	{
		Log::err("Physics: Failed to allocate PhysicsConvexMeshHandle!");
		return {};
	}

	// create and store convex mesh
	{
		///LOCK_HOLDER(m_materialsMutex);
		if (handle > m_convexMeshes.size())
		{
			m_convexMeshes.resize((size_t)(m_convexMeshes.size() * 1.5));
		}

		// unfortunately we need to cast the constness away, but it should still be ok because the PxDefaultMemoryInputData
		// constructor directly assigns it to a const member.
		PxDefaultMemoryInputData input(const_cast<PxU8 *>(reinterpret_cast<const PxU8 *>(data)), size);
		auto *mesh = m_pxPhysics->createConvexMesh(input);
		m_convexMeshes[handle - 1] = mesh;
	}

	return handle;
}

void Physics::destroyConvexMesh(PhysicsConvexMeshHandle handle) noexcept
{
	// LOCK_HOLDER(m_materialsMutex);
	{
		const bool validHandle = handle != 0 && handle <= m_convexMeshes.size();

		if (!validHandle)
		{
			return;
		}

		PX_RELEASE(m_convexMeshes[handle - 1]);

		{
			//LOCK_HOLDER(m_handleManagerMutex);
			m_convexMeshHandleManager.free(handle);
		}
	}
}

PhysicsTriangleMeshHandle Physics::createTriangleMesh(uint32_t size, const char *data) noexcept
{
	PhysicsTriangleMeshHandle handle{};

	// LOCK_HOLDER(m_materialsMutex);

	// allocate handle
	{
		// LOCK_HOLDER(m_handleManagerMutex);
		handle = (PhysicsTriangleMeshHandle)m_triangleMeshHandleManager.allocate();
	}

	if (!handle)
	{
		Log::err("Physics: Failed to allocate PhysicsTriangleMeshHandle!");
		return {};
	}

	// create and store convex mesh
	{
		///LOCK_HOLDER(m_materialsMutex);
		if (handle > m_triangleMeshes.size())
		{
			m_triangleMeshes.resize((size_t)(m_triangleMeshes.size() * 1.5));
		}

		// unfortunately we need to cast the constness away, but it should still be ok because the PxDefaultMemoryInputData
		// constructor directly assigns it to a const member.
		PxDefaultMemoryInputData input(const_cast<PxU8 *>(reinterpret_cast<const PxU8 *>(data)), size);
		auto *mesh = m_pxPhysics->createTriangleMesh(input);
		m_triangleMeshes[handle - 1] = mesh;
	}

	return handle;
}

void Physics::destroyTriangleMesh(PhysicsTriangleMeshHandle handle) noexcept
{
	// LOCK_HOLDER(m_materialsMutex);
	{
		const bool validHandle = handle != 0 && handle <= m_triangleMeshes.size();

		if (!validHandle)
		{
			return;
		}

		PX_RELEASE(m_triangleMeshes[handle - 1]);

		{
			//LOCK_HOLDER(m_handleManagerMutex);
			m_triangleMeshHandleManager.free(handle);
		}
	}
}

bool Physics::cookConvexMesh(uint32_t vertexCount, const float *vertices, uint32_t *resultBufferSize, char **resultBuffer) noexcept
{
	Log::info("Physics: Cooking convex mesh.");

	PxConvexMeshDesc convexDesc{};
	convexDesc.points.count = vertexCount;
	convexDesc.points.stride = sizeof(float) * 3;
	convexDesc.points.data = vertices;
	convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	PxDefaultMemoryOutputStream buf;
	PxConvexMeshCookingResult::Enum result;
	if (!m_pxCooking->cookConvexMesh(convexDesc, buf, &result))
	{
		Log::err("Physics: Cooking physics convex mesh failed!");
		return false;
	}

	*resultBuffer = new char[buf.getSize()];
	*resultBufferSize = buf.getSize();

	memcpy(*resultBuffer, buf.getData(), buf.getSize());

	Log::info("Physics: Cooking convex mesh finished.");

	return true;
}

bool Physics::cookTriangleMesh(uint32_t indexCount, const uint32_t *indices, uint32_t vertexCount, const float *vertices, uint32_t *resultBufferSize, char **resultBuffer) noexcept
{
	Log::info("Physics: Cooking triangle mesh.");

	assert(indexCount % 3 == 0);

	PxTriangleMeshDesc meshDesc{};
	meshDesc.points.count = vertexCount;
	meshDesc.points.stride = sizeof(float) * 3;
	meshDesc.points.data = vertices;
	meshDesc.triangles.count = indexCount / 3;
	meshDesc.triangles.stride = sizeof(indices[0]) * 3;
	meshDesc.triangles.data = indices;

	PxDefaultMemoryOutputStream buf;
	PxTriangleMeshCookingResult::Enum result;
	if (!m_pxCooking->cookTriangleMesh(meshDesc, buf, &result))
	{
		Log::err("Physics: Cooking physics triangle mesh failed!");
		return false;
	}

	if (result == PxTriangleMeshCookingResult::Enum::eLARGE_TRIANGLE)
	{
		Log::warn("Physics: Encountered a triangle during mesh cooking that is too large for well-conditioned results!");
	}

	*resultBuffer = new char[buf.getSize()];
	*resultBufferSize = buf.getSize();

	memcpy(*resultBuffer, buf.getData(), buf.getSize());

	Log::info("Physics: Cooking triangle mesh finished.");

	return true;
}
