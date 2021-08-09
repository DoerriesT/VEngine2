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
#include <component/CharacterMovementComponent.h>
#include <component/InputStateComponent.h>
#include <asset/AssetManager.h>
#include <Editor.h>
#include <graphics/Renderer.h>
#include <input/FPSCameraController.h>
#include <input/ThirdPersonCameraController.h>
#include <graphics/Camera.h>
#include <Level.h>
#include <physics/Physics.h>
#include "CustomAnimGraph.h"

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
		m_thirdPersonCameraController = new ThirdPersonCameraController(m_engine->getECS());

		PhysicsMaterialCreateInfo matCreateInfo{ 0.5f, 0.5f, 0.6f };
		m_engine->getPhysics()->createMaterials(1, &matCreateInfo, &m_physicsMaterial);

		// camera
		{
			TransformComponent transC1{};
			transC1.m_translation = glm::vec3(0.0f, 2.0f, 12.0f);

			CameraComponent cameraC1{};
			cameraC1.m_fovy = glm::radians(60.0f);



			Camera cam(transC1, cameraC1);
			m_cameraEntity = m_engine->getECS()->createEntity<TransformComponent, CameraComponent>(transC1, cameraC1);
			m_engine->getRenderer()->setCameraEntity(m_cameraEntity);
			m_engine->getLevel()->addEntity(m_cameraEntity, "Camera");
		}

		// player character
		{
			m_cesiumManAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/character"));
			m_skeleton = AssetManager::get()->getAsset<SkeletonAssetData>(SID("meshes/character.skel"));
			m_idleClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/idle.anim"));
			m_walkClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/walk0.anim"));
			m_runClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/run.anim"));
			m_strafeLeftWalkClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/strafe_left_walk.anim"));
			m_strafeRightWalkClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/strafe_right_walk.anim"));
			m_strafeLeftRunClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/strafe_left_run.anim"));
			m_strafeRightRunClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/strafe_right_run.anim"));

			m_customAnimGraph = new CustomAnimGraph(
				m_idleClip,
				m_walkClip,
				m_runClip,
				m_strafeLeftWalkClip,
				m_strafeRightWalkClip,
				m_strafeLeftRunClip,
				m_strafeRightRunClip
			);

			TransformComponent transC{};
			transC.m_rotation = glm::quat(glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f));
			transC.m_scale = glm::vec3(0.01f);
			transC.m_translation.y = 4.0f;

			SkinnedMeshComponent meshC{};
			meshC.m_mesh = m_cesiumManAsset;
			meshC.m_skeleton = m_skeleton;
			meshC.m_animationGraph = m_customAnimGraph;

			CharacterControllerComponent ccC{};
			ccC.m_translationHeightOffset = 0.0f;
			ccC.m_capsuleRadius = 0.3f;
			CharacterMovementComponent movC{};
			movC.m_active = true;

			m_playerEntity = m_engine->getECS()->createEntity<TransformComponent, SkinnedMeshComponent, CharacterControllerComponent, CharacterMovementComponent>(transC, meshC, ccC, movC);
			m_engine->getLevel()->addEntity(m_playerEntity, "Player");
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

		// sponza
		{
			m_sponzaAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/sponza"));

			TransformComponent transC{};
			transC.m_translation.z = -30.0f;

			MeshComponent meshC{ m_sponzaAsset };

			PhysicsComponent physicsC{};
			physicsC.m_mobility = PhysicsMobility::STATIC;
			physicsC.m_physicsShapeType = PhysicsShapeType::TRIANGLE_MESH;
			physicsC.m_triangleMeshHandle = m_sponzaAsset->getPhysicsTriangleMeshhandle();
			physicsC.m_materialHandle = m_physicsMaterial;

			auto entity = m_engine->getECS()->createEntity<TransformComponent, MeshComponent, PhysicsComponent>(transC, meshC, physicsC);
			m_engine->getLevel()->addEntity(entity, "Sponza");
		}

		//// cesium man
		//{
		//	m_cesiumManAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/character"));
		//	m_skeleton = AssetManager::get()->getAsset<SkeletonAssetData>(SID("meshes/character.skel"));
		//	m_idleClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/idle.anim"));
		//	m_walkClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/walking.anim"));
		//	m_runClip = AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/running0.anim"));
		//
		//	m_customAnimGraph = new CustomAnimGraph(m_idleClip, m_walkClip, m_runClip);
		//
		//	TransformComponent transC{};
		//	transC.m_rotation = glm::quat(glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f));
		//	transC.m_scale = glm::vec3(0.01f);
		//
		//	SkinnedMeshComponent meshC{};
		//	meshC.m_mesh = m_cesiumManAsset;
		//	meshC.m_skeleton = m_skeleton;
		//	meshC.m_animationGraph = m_customAnimGraph;
		//
		//	//PhysicsComponent physicsC{};
		//	//physicsC.m_mobility = PhysicsMobility::DYNAMIC;
		//	//physicsC.m_physicsShapeType = PhysicsShapeType::CONVEX_MESH;
		//	//physicsC.m_convexMeshHandle = m_cesiumManAsset->getPhysicsConvexMeshhandle();
		//	//physicsC.m_materialHandle = m_physicsMaterial;
		//
		//	m_manEntity = m_engine->getECS()->createEntity<TransformComponent, SkinnedMeshComponent>(transC, meshC);
		//	m_engine->getLevel()->addEntity(m_manEntity, "Cesium Man");
		//}

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

	void setPlaying(bool playing) noexcept override
	{
		m_playing = playing;
	}

	void update(float deltaTime) noexcept override
	{
		if (m_playing)
		{
			m_engine->getRenderer()->setCameraEntity(m_cameraEntity);

			TransformComponent *tc = m_engine->getECS()->getComponent<TransformComponent>(m_cameraEntity);
			CameraComponent *cc = m_engine->getECS()->getComponent<CameraComponent>(m_cameraEntity);
			assert(tc && cc);

			uint32_t swWidth, swHeight, w, h;
			m_engine->getResolution(&swWidth, &swHeight, &w, &h);

			// make sure aspect ratio of editor camera is correct
			cc->m_aspectRatio = (w > 0 && h > 0) ? w / (float)h : 1.0f;

			Camera camera(*tc, *cc);

			auto *playerMovementComponent = m_engine->getECS()->getComponent<CharacterMovementComponent>(m_playerEntity);
			m_thirdPersonCameraController->update(deltaTime, camera, m_engine->getECS()->getComponent<TransformComponent>(m_playerEntity), playerMovementComponent, m_engine->getPhysics());

			glm::vec2 playerMovementDir = glm::vec2(playerMovementComponent->m_movementRightInputAxis, playerMovementComponent->m_movementForwardInputAxis);
			float playerMovementSpeed = glm::length(playerMovementDir) / deltaTime / 6.0f;
			playerMovementSpeed = playerMovementComponent->m_movementForwardInputAxis < 0.0f ? -playerMovementSpeed : playerMovementSpeed;
			m_customAnimGraph->m_speed = glm::isnan(playerMovementSpeed) ? 0.0f : playerMovementSpeed;
			glm::vec2 direction = glm::normalize(playerMovementDir);
			m_customAnimGraph->m_strafe = glm::isnan(direction.x) ? 0.0f : direction.x;

			const auto *inputState = m_engine->getECS()->getSingletonComponent<InputStateComponent>();

			if (inputState->m_shootAction.m_pressed)
			{
				glm::vec3 velocity;
				glm::vec3 pos;
				{
					auto d = camera.getForwardDirection();
					velocity = d * 20.0f;
					pos = tc->m_translation + d;
				}

				createPhysicsObject(pos, velocity, PhysicsMobility::DYNAMIC);
			}
		}


		//auto *manTc = m_engine->getECS()->getComponent<TransformComponent>(m_manEntity);
		//glm::vec3 rootMotion = glm::vec3(0.0f, 0.0f, 1.0f) * 0.01f * m_customAnimGraph->getRootMotionSpeed();
		//manTc->m_translation += rootMotion;

		ImGui::Begin("Custom Animation Graph Controls");

		ImGui::SliderFloat("Phase", &m_customAnimGraph->m_phase, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat("Speed", &m_customAnimGraph->m_speed, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::Checkbox("Active", &m_customAnimGraph->m_active);
		ImGui::Checkbox("Paused", &m_customAnimGraph->m_paused);
		ImGui::Checkbox("Looping", &m_customAnimGraph->m_loop);

		ImGui::End();
	}

	void shutdown() noexcept override
	{
		for (auto n : m_engine->getLevel()->getSceneGraphNodes())
		{
			m_engine->getECS()->destroyEntity(n->m_entity);
		}

		delete m_customAnimGraph;
		m_customAnimGraph = nullptr;

		delete m_fpsCameraController;
		m_fpsCameraController = nullptr;
		delete m_thirdPersonCameraController;
		m_thirdPersonCameraController = nullptr;

		m_meshAsset.release();
		m_sponzaAsset.release();
		m_cesiumManAsset.release();
		m_skeleton.release();
		m_idleClip.release();
		m_walkClip.release();
		m_runClip.release();
		m_strafeLeftWalkClip.release();
		m_strafeRightWalkClip.release();
		m_strafeLeftRunClip.release();
		m_strafeRightRunClip.release();
	}

private:
	Engine *m_engine = nullptr;
	FPSCameraController *m_fpsCameraController = nullptr;
	ThirdPersonCameraController *m_thirdPersonCameraController = nullptr;
	Asset<MeshAssetData> m_meshAsset;
	Asset<MeshAssetData> m_sponzaAsset;
	Asset<MeshAssetData> m_cesiumManAsset;
	Asset<SkeletonAssetData> m_skeleton;
	Asset<AnimationClipAssetData> m_idleClip;
	Asset<AnimationClipAssetData> m_walkClip;
	Asset<AnimationClipAssetData> m_runClip;
	Asset<AnimationClipAssetData> m_strafeLeftWalkClip;
	Asset<AnimationClipAssetData> m_strafeRightWalkClip;
	Asset<AnimationClipAssetData> m_strafeLeftRunClip;
	Asset<AnimationClipAssetData> m_strafeRightRunClip;
	EntityID m_cameraEntity;
	EntityID m_manEntity;
	EntityID m_playerEntity;
	PhysicsMaterialHandle m_physicsMaterial = {};
	CustomAnimGraph *m_customAnimGraph = {};
	bool m_playing = true;

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