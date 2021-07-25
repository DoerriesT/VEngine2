#include "Physics.h"
#include "Log.h"
#include <assert.h>
#include <PxPhysicsAPI.h>
#include "ecs/ECS.h"
#include "component/PhysicsComponent.h"
#include "component/TransformComponent.h"

#define PX_RELEASE(x)	if(x)	{ x->release(); x = NULL;	}

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
	m_pxCpuDispatcher = PxDefaultCpuDispatcherCreate(2);
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

	m_materials.resize(16);
}

Physics::~Physics() noexcept
{
	PX_RELEASE(m_pxPhysics);
	PX_RELEASE(m_pxFoundation);
}

void Physics::update(float deltaTime) noexcept
{
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
					const bool validMaterialHandle = pc.m_materialHandle != 0 && pc.m_materialHandle <= m_materials.size();
					if (!validMaterialHandle)
					{
						// TODO: some error message
						continue;
					}

					// fetch material
					PxMaterial *mat = m_materials[pc.m_materialHandle - 1];

					// create transform
					PxTransform pxTransform{};
					pxTransform.p.x = tc.m_translation.x;
					pxTransform.p.y = tc.m_translation.y;
					pxTransform.p.z = tc.m_translation.z;
					pxTransform.q.x = tc.m_rotation.x;
					pxTransform.q.y = tc.m_rotation.y;
					pxTransform.q.z = tc.m_rotation.z;
					pxTransform.q.w = tc.m_rotation.w;


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

					m_pxScene->addActor(*actor);

					pc.m_internalPhysicsActorHandle = actor;
				}

				// set kinematic transform
				if (pc.m_mobility == PhysicsMobility::KINEMATIC)
				{
					// create transform
					PxTransform pxTransform{};
					pxTransform.p.x = tc.m_translation.x;
					pxTransform.p.y = tc.m_translation.y;
					pxTransform.p.z = tc.m_translation.z;
					pxTransform.q.x = tc.m_rotation.x;
					pxTransform.q.y = tc.m_rotation.y;
					pxTransform.q.z = tc.m_rotation.z;
					pxTransform.q.w = tc.m_rotation.w;

					((PxRigidDynamic *)pc.m_internalPhysicsActorHandle)->setKinematicTarget(pxTransform);
				}
			}
		});

	// step the simulation
	constexpr float k_stepSize = 1.0f / 60.0f;
	m_timeAccumulator += deltaTime;
	while (m_timeAccumulator >= k_stepSize)
	{
		m_timeAccumulator -= k_stepSize;
		m_pxScene->simulate(k_stepSize);
		m_pxScene->fetchResults(true);
	}
	

	// sync entities with physics state
	m_ecs->iterate<TransformComponent, PhysicsComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, PhysicsComponent *physicsC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &pc = physicsC[i];
				
				if (pc.m_mobility == PhysicsMobility::DYNAMIC)
				{
					auto pxTc = ((PxRigidDynamic *)pc.m_internalPhysicsActorHandle)->getGlobalPose();
					tc.m_translation.x = pxTc.p.x;
					tc.m_translation.y = pxTc.p.y;
					tc.m_translation.z = pxTc.p.z;
					tc.m_rotation.x = pxTc.q.x;
					tc.m_rotation.y = pxTc.q.y;
					tc.m_rotation.z = pxTc.q.z;
					tc.m_rotation.w = pxTc.q.w;
				}
			}
		});
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
			Log::err("Physics: Failed to PhysicsMaterialHandle MaterialHandle!");
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
