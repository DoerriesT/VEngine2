#include "Engine.h"
#include <EASTL/fixed_vector.h>
#include <task/Task.h>
#include <stdio.h>
#include <ecs/ECS.h>
#include <ecs/ECSTypeIDTranslator.h>
#include <reflection/Reflection.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <IGameLogic.h>
#include <graphics/imgui/imgui.h>
#include <component/TransformComponent.h>
#include <component/CameraComponent.h>
#include <component/LightComponent.h>
#include <component/MeshComponent.h>
#include <component/SkinnedMeshComponent.h>
#include <component/PhysicsComponent.h>
#include <component/CharacterControllerComponent.h>
#include <component/PlayerMovementComponent.h>
#include <component/InputStateComponent.h>
#include <asset/AssetManager.h>
#include <Editor.h>
#include <graphics/Renderer.h>
#include <input/FPSCameraController.h>
#include <graphics/Camera.h>
#include <Level.h>
#include <physics/Physics.h>

void visitType(const Reflection &r, const TypeID &typeID, const char *fieldName = nullptr, unsigned int indent = 0)
{
	for (size_t i = 0; i < indent; ++i)
	{
		printf(" ");
	}

	auto type = r.getType(typeID);
	printf("%s %s\n", type->m_name, fieldName ? fieldName : "");

	if (type->m_typeCategory == TYPE_CATEGORY_POINTER || type->m_typeCategory == TYPE_CATEGORY_ARRAY)
	{
		visitType(r, type->m_pointedToTypeID, nullptr, indent + 4);
	}
	else
	{
		for (auto f : type->m_fields)
		{
			visitType(r, f.m_typeID, f.m_name, indent + 4);
		}
	}
	
}

class DummyGameLogic : public IGameLogic
{
public:
	void init(Engine *engine) noexcept override
	{
		m_engine = engine;
		m_fpsCameraController = new FPSCameraController(m_engine->getECS());

		PhysicsMaterialCreateInfo matCreateInfo{ 0.5f, 0.5f, 0.6f };
		m_engine->getPhysics()->createMaterials(1, &matCreateInfo, &m_physicsMaterial);

		// camera
		{
			TransformComponent transC1{};
			transC1.m_translation = glm::vec3(0.0f, 2.0f, 12.0f);
			
			CameraComponent cameraC1{};
			cameraC1.m_fovy = glm::radians(60.0f);

			//PhysicsComponent physicsC{};
			//physicsC.m_mobility = PhysicsMobility::KINEMATIC;
			//physicsC.m_physicsShapeType = PhysicsShapeType::SPHERE;
			//physicsC.m_sphereRadius = 0.5f;
			//physicsC.m_materialHandle = m_physicsMaterial;

			CharacterControllerComponent ccC{};
			PlayerMovementComponent pmC{};
			pmC.m_active = true;

			Camera cam(transC1, cameraC1);
			m_cameraEntity = m_engine->getECS()->createEntity<TransformComponent, CameraComponent, CharacterControllerComponent, PlayerMovementComponent>(transC1, cameraC1, ccC, pmC);
			m_engine->getRenderer()->setCameraEntity(m_cameraEntity);
			m_engine->getLevel()->addEntity(m_cameraEntity, "First Person Camera");
		}

		// ground plane
		{
			TransformComponent transC{};
		
			PhysicsComponent physicsC{};
			physicsC.m_mobility = PhysicsMobility::STATIC;
			physicsC.m_physicsShapeType = PhysicsShapeType::PLANE;
			physicsC.m_materialHandle = m_physicsMaterial;
		
			auto entity = m_engine->getECS()->createEntity<TransformComponent, PhysicsComponent>(transC, physicsC);
			m_engine->getLevel()->addEntity(entity, "Ground Plane");
		}
		
		//// sponza
		//{
		//	m_sponzaAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/sponza"));
		//
		//	TransformComponent transC{};
		//
		//	MeshComponent meshC{ m_sponzaAsset };
		//
		//	PhysicsComponent physicsC{};
		//	physicsC.m_mobility = PhysicsMobility::STATIC;
		//	physicsC.m_physicsShapeType = PhysicsShapeType::TRIANGLE_MESH;
		//	physicsC.m_triangleMeshHandle = m_sponzaAsset->getPhysicsTriangleMeshhandle();
		//	physicsC.m_materialHandle = m_physicsMaterial;
		//
		//	auto entity = m_engine->getECS()->createEntity<TransformComponent, MeshComponent, PhysicsComponent>(transC, meshC, physicsC);
		//	m_engine->getLevel()->addEntity(entity, "Sponza");
		//}

		// cesium man
		{
			m_cesiumManAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/CesiumMan"));
			m_skeleton = AssetManager::get()->getAsset<SkeletonAssetData>(SID("meshes/CesiumMan_skeleton0.skel"));
			m_animationClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/CesiumMananimation_clip0.anim"));
		
			TransformComponent transC{};
			transC.m_rotation = glm::quat(glm::vec3(-glm::half_pi<float>(), 0.0f, 0.0f));
		
			SkinnedMeshComponent meshC{ m_cesiumManAsset, m_skeleton, m_animationClip, 0.0f, true, true };

			PhysicsComponent physicsC{};
			physicsC.m_mobility = PhysicsMobility::DYNAMIC;
			physicsC.m_physicsShapeType = PhysicsShapeType::CONVEX_MESH;
			physicsC.m_convexMeshHandle = m_cesiumManAsset->getPhysicsConvexMeshhandle();
			physicsC.m_materialHandle = m_physicsMaterial;
		
			auto entity = m_engine->getECS()->createEntity<TransformComponent, SkinnedMeshComponent, PhysicsComponent>(transC, meshC, physicsC);
			m_engine->getLevel()->addEntity(entity, "Cesium Man");
		}

		// mesh
		{
			m_meshAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/uvsphere"));

			//for (int i = 0; i < 10; ++i)
			//{
			//	TransformComponent transC{};
			//	transC.m_translation.y = 1.1f;// 10.0f + i * 2.5f;
			//	transC.m_translation.x = ((rand() / (float)RAND_MAX) - 0.5f) * 20.0f;
			//	transC.m_translation.z = ((rand() / (float)RAND_MAX) - 0.5f) * 20.0f;
			//
			//	createPhysicsObject(transC.m_translation, {}, PhysicsMobility::DYNAMIC);
			//}
		}
	}

	void update(float deltaTime) noexcept override
	{
		m_activeCamera = m_engine->getRenderer()->getCameraEntity();
		m_engine->getRenderer()->setCameraEntity(m_cameraEntity);

		TransformComponent *tc = m_engine->getECS()->getComponent<TransformComponent>(m_cameraEntity);
		CameraComponent *cc = m_engine->getECS()->getComponent<CameraComponent>(m_cameraEntity);
		assert(tc && cc);

		uint32_t swWidth, swHeight, w, h;
		m_engine->getResolution(&swWidth, &swHeight, &w, &h);

		// make sure aspect ratio of editor camera is correct
		cc->m_aspectRatio = (w > 0 && h > 0) ? w / (float)h : 1.0f;

		Camera camera(*tc, *cc);

		m_fpsCameraController->update(deltaTime, camera, m_engine->getECS()->getComponent<CharacterControllerComponent>(m_cameraEntity));

		const auto *inputState = m_engine->getECS()->getSingletonComponent<RawInputStateComponent>();

		if (inputState->isKeyPressed(InputKey::F))
		{
			glm::vec3 velocity;
			glm::vec3 pos;
			{
				TransformComponent *tc = m_engine->getECS()->getComponent<TransformComponent>(m_cameraEntity);
				CameraComponent *cc = m_engine->getECS()->getComponent<CameraComponent>(m_cameraEntity);
				Camera cam(*tc, *cc);

				auto d = cam.getForwardDirection();
				velocity = d * 20.0f;
				pos = tc->m_translation + d;
			}

			createPhysicsObject(pos, velocity, PhysicsMobility::DYNAMIC);
		}
	}

	void shutdown() noexcept override
	{
		for (auto n : m_engine->getLevel()->getSceneGraphNodes())
		{
			m_engine->getECS()->destroyEntity(n->m_entity);
		}

		m_meshAsset.release();
		m_sponzaAsset.release();
		m_cesiumManAsset.release();
		m_skeleton.release();
		m_animationClip.release();
	}

private:
	Engine *m_engine = nullptr;
	FPSCameraController *m_fpsCameraController = nullptr;
	Asset<MeshAssetData> m_meshAsset;
	Asset<MeshAssetData> m_sponzaAsset;
	Asset<MeshAssetData> m_cesiumManAsset;
	Asset<SkeletonAssetData> m_skeleton;
	Asset<AnimationClipAssetData> m_animationClip;
	EntityID m_cameraEntity;
	EntityID m_activeCamera;
	PhysicsMaterialHandle m_physicsMaterial = {};

	void createPhysicsObject(const glm::vec3 &pos, const glm::vec3 &vel, PhysicsMobility mobility)
	{
		TransformComponent transC{};
		transC.m_translation.x = pos.x;
		transC.m_translation.y = pos.y;
		transC.m_translation.z = pos.z;
		transC.m_scale = glm::vec3(0.25f);

		MeshComponent meshC{ m_meshAsset };

		PhysicsComponent physicsC{};
		physicsC.m_mobility = mobility;
		physicsC.m_physicsShapeType = PhysicsShapeType::SPHERE;
		physicsC.m_sphereRadius = 0.25f;
		physicsC.m_linearDamping = 0.0f;
		physicsC.m_angularDamping = 0.5f;
		physicsC.m_initialVelocityX = vel.x;
		physicsC.m_initialVelocityY = vel.y;
		physicsC.m_initialVelocityZ = vel.z;
		physicsC.m_materialHandle = m_physicsMaterial;

		auto entity = m_engine->getECS()->createEntity<TransformComponent, MeshComponent, PhysicsComponent>(transC, meshC, physicsC);
		m_engine->getLevel()->addEntity(entity, "Physics Sphere");
	}
};



int main(int argc, char *argv[])
{
	DummyGameLogic gameLogic;
	Editor editor(&gameLogic);
	Engine engine;
	return engine.start(argc, argv, &editor);
}