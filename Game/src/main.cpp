#define NOMINMAX
#include "Engine.h"
#include <EASTL/fixed_vector.h>
#include <stdio.h>
#include <ecs/ECS.h>
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
#include <component/RawInputStateComponent.h>
#include <component/ReflectionProbeComponent.h>
#include <asset/AssetManager.h>
#include <Editor.h>
#include <graphics/Renderer.h>
#include <input/FPSCameraController.h>
#include <input/ThirdPersonCameraController.h>
#include <graphics/Camera.h>
#include <Level.h>
#include <physics/Physics.h>
#include <animation/AnimationGraph.h>
#include <filesystem/VirtualFileSystem.h>
#include "Log.h"
#include <iostream>
#include <filesystem/Path.h>
#include <profiling/Profiling.h>

static AnimationGraph *setupAnimationGraph()
{
	Asset<AnimationClipAssetData> animClips[]
	{
		AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/idle.anim")),
		AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/walk0.anim")),
		AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/run.anim"))
	};

	AnimationGraphParameter params[2];
	params[0].m_type = AnimationGraphParameter::Type::BOOL;
	params[0].m_data.b = true;
	params[0].m_name = SID("loop");
	params[1].m_type = AnimationGraphParameter::Type::FLOAT;
	params[1].m_data.f = 0.0f;
	params[1].m_name = SID("speed");

	AnimationGraphNode nodes[4] = {};

	// 1D array lerp node
	nodes[0].m_nodeType = AnimationGraphNodeType::LERP_1D_ARRAY;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputs[0] = 1;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputs[1] = 2;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputs[2] = 3;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[0] = 0.0f;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[1] = 2.0f;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[2] = 6.0f;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputCount = 3;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_alpha = 1;

	// idle clip node
	nodes[1].m_nodeType = AnimationGraphNodeType::ANIM_CLIP;
	nodes[1].m_nodeData.m_clipNodeData.m_animClip = 0;
	nodes[1].m_nodeData.m_clipNodeData.m_loop = 0;

	// walk clip node
	nodes[2].m_nodeType = AnimationGraphNodeType::ANIM_CLIP;
	nodes[2].m_nodeData.m_clipNodeData.m_animClip = 1;
	nodes[2].m_nodeData.m_clipNodeData.m_loop = 0;

	// run clip node
	nodes[3].m_nodeType = AnimationGraphNodeType::ANIM_CLIP;
	nodes[3].m_nodeData.m_clipNodeData.m_animClip = 2;
	nodes[3].m_nodeData.m_clipNodeData.m_loop = 0;

	return new AnimationGraph(0, eastl::size(nodes), nodes, eastl::size(params), params, eastl::size(animClips), animClips, AssetManager::get()->getAsset<ScriptAssetData>(SID("scripts/anim_graph.lua")));
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
			transC1.m_transform.m_translation = glm::vec3(0.0f, 2.0f, 12.0f);

			CameraComponent cameraC1{};
			cameraC1.m_fovy = glm::radians(60.0f);



			Camera cam(transC1, cameraC1);
			m_cameraEntity = m_engine->getECS()->createEntity<TransformComponent, CameraComponent>(transC1, cameraC1);
			m_engine->setCameraEntity(m_cameraEntity);
			m_engine->getLevel()->addEntity(m_cameraEntity, "Camera");
		}

		// player character
		{
			m_cesiumManAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/character.mesh"));
			m_skeleton = AssetManager::get()->getAsset<SkeletonAssetData>(SID("meshes/character.skel"));

			m_customAnimGraph = setupAnimationGraph();

			TransformComponent transC{};
			transC.m_transform.m_rotation = glm::quat(glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f));
			transC.m_transform.m_scale = glm::vec3(0.01f);
			transC.m_transform.m_translation.y = 4.0f;

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
			m_sponzaAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/sponza.mesh"));

			TransformComponent transC{};
			transC.m_transform.m_translation = glm::vec3(0.65f, 0.0f, 0.35f);

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
			m_meshAsset = AssetManager::get()->getAsset<MeshAssetData>(SID("meshes/uvsphere.mesh"));

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

		// light
		{
			TransformComponent transC{};

			LightComponent lightC{};
			lightC.m_type = LightComponent::Type::Directional;
			lightC.m_shadows = true;
			lightC.m_cascadeCount = 4;
			lightC.m_intensity = 10.0f;

			auto entity = m_engine->getECS()->createEntity<TransformComponent, LightComponent>(transC, lightC);
			m_engine->getLevel()->addEntity(entity, "Directional Light");
		}

		// reflection probes
		{
			auto createReflectionProbe = [&](const glm::vec3 &bboxMin, const glm::vec3 &bboxMax, bool manualOffset = false, const glm::vec3 &capturePos = glm::vec3(0.0f))
			{
				glm::vec3 probeCenter = (bboxMin + bboxMax) * 0.5f;
				glm::vec3 captureOffset = manualOffset ? capturePos - probeCenter : glm::vec3(0.0f);

				TransformComponent transC{};
				transC.m_transform.m_translation = probeCenter;
				transC.m_transform.m_scale = bboxMax - probeCenter;

				ReflectionProbeComponent probeC{};
				probeC.m_captureOffset = captureOffset;

				auto reflectionProbeEntity = m_engine->getECS()->createEntity<TransformComponent, ReflectionProbeComponent>(transC, probeC);
				m_engine->getLevel()->addEntity(reflectionProbeEntity, "Reflection Probe");
				return reflectionProbeEntity;
			};

			// center
			createReflectionProbe(glm::vec3(-9.5f, -0.05f, -2.4f), glm::vec3(9.5f, 13.0f, 2.4f), true, glm::vec3(0.0f, 2.0f, 0.0f));

			// lower halls
			createReflectionProbe(glm::vec3(-9.5, -0.05, 2.4), glm::vec3(9.5, 3.9, 6.1));
			createReflectionProbe(glm::vec3(-9.5, -0.05, -6.1), glm::vec3(9.5, 3.9, -2.4));

			// lower end
			createReflectionProbe(glm::vec3(-13.7, -0.05, -6.1), glm::vec3(-9.5, 3.9, 6.1));
			createReflectionProbe(glm::vec3(9.5, -0.05, -6.1), glm::vec3(13.65, 3.9, 6.1));

			// upper halls
			createReflectionProbe(glm::vec3(-9.8, 4.15, 2.8), glm::vec3(9.8, 8.7, 6.15));
			createReflectionProbe(glm::vec3(-9.8, 4.15, -6.1), glm::vec3(9.8, 8.7, -2.8));

			// upper ends
			createReflectionProbe(glm::vec3(-13.7, 4.15, -6.1), glm::vec3(-9.8, 8.7, 6.15));
			createReflectionProbe(glm::vec3(9.8, 4.15, -6.1), glm::vec3(13.65, 8.7, 6.15));
		}
	}

	void setPlaying(bool playing) noexcept override
	{
		m_playing = playing;
	}

	void update(float deltaTime) noexcept override
	{
		PROFILING_ZONE_SCOPED;

		if (m_playing)
		{
			m_engine->setCameraEntity(m_cameraEntity);

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

			const auto *inputState = m_engine->getECS()->getSingletonComponent<InputStateComponent>();

			if (inputState->m_shootAction.m_pressed)
			{
				glm::vec3 velocity;
				glm::vec3 pos;
				{
					auto d = camera.getForwardDirection();
					velocity = d * 20.0f;
					pos = tc->m_transform.m_translation + d;
				}

				createPhysicsObject(pos, velocity, PhysicsMobility::DYNAMIC);
			}
		}


		//auto *manTc = m_engine->getECS()->getComponent<TransformComponent>(m_manEntity);
		//glm::vec3 rootMotion = glm::vec3(0.0f, 0.0f, 1.0f) * 0.01f * m_customAnimGraph->getRootMotionSpeed();
		//manTc->m_translation += rootMotion;

		//ImGui::Begin("Custom Animation Graph Controls");
		//
		//ImGui::SliderFloat("Phase", &m_customAnimGraph->m_phase, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		//ImGui::SliderFloat("Speed", &m_customAnimGraph->m_speed, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		//ImGui::Checkbox("Active", &m_customAnimGraph->m_active);
		//ImGui::Checkbox("Paused", &m_customAnimGraph->m_paused);
		//
		//ImGui::End();
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
	}

private:
	Engine *m_engine = nullptr;
	FPSCameraController *m_fpsCameraController = nullptr;
	ThirdPersonCameraController *m_thirdPersonCameraController = nullptr;
	Asset<MeshAssetData> m_meshAsset;
	Asset<MeshAssetData> m_sponzaAsset;
	Asset<MeshAssetData> m_cesiumManAsset;
	Asset<SkeletonAssetData> m_skeleton;
	EntityID m_cameraEntity;
	EntityID m_manEntity;
	EntityID m_playerEntity;
	PhysicsMaterialHandle m_physicsMaterial = {};
	AnimationGraph *m_customAnimGraph = {};
	bool m_playing = true;

	void createPhysicsObject(const glm::vec3 &pos, const glm::vec3 &vel, PhysicsMobility mobility)
	{
		TransformComponent transC{};
		transC.m_transform.m_translation.x = pos.x;
		transC.m_transform.m_translation.y = pos.y;
		transC.m_transform.m_translation.z = pos.z;
		transC.m_transform.m_scale = glm::vec3(0.25f);

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