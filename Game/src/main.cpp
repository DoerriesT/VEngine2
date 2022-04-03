#define NOMINMAX
#include "Engine.h"
#include <EASTL/fixed_vector.h>
#include <stdio.h>
#include <ecs/ECS.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <IGameLogic.h>
#include <graphics/imgui/imgui.h>
#include <component/EntityMetaComponent.h>
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
#include <component/IrradianceVolumeComponent.h>
#include <asset/AssetManager.h>
#include <Editor.h>
#include <graphics/Renderer.h>
#include <input/FPSCameraController.h>
#include <input/ThirdPersonCameraController.h>
#include <graphics/Camera.h>
#include <physics/Physics.h>
#include <animation/AnimationGraph.h>
#include <filesystem/VirtualFileSystem.h>
#include "Log.h"
#include <iostream>
#include <filesystem/Path.h>
#include <profiling/Profiling.h>
#include <TransformHierarchy.h>

static AnimationGraph *setupAnimationGraph()
{
	Asset<AnimationClipAsset> animClips[]
	{
		AssetManager::get()->getAsset<AnimationClipAsset>(SID("meshes/idle.anim")),
		AssetManager::get()->getAsset<AnimationClipAsset>(SID("meshes/walk0.anim")),
		AssetManager::get()->getAsset<AnimationClipAsset>(SID("meshes/run.anim"))
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
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[1] = 1.0f;
	nodes[0].m_nodeData.m_lerp1DArrayNodeData.m_inputKeys[2] = 4.0f;
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

	return new AnimationGraph(0, eastl::size(nodes), nodes, eastl::size(params), params, eastl::size(animClips), animClips, AssetManager::get()->getAsset<ScriptAsset>(SID("scripts/anim_graph.lua")));
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
			Transform transform{};
			transform.m_translation = glm::vec3(-12.0f, 2.0f, 0.0f);
			TransformComponent transC1(transform, Mobility::Dynamic);

			CameraComponent cameraC1{};
			cameraC1.m_fovy = glm::radians(60.0f);

			m_cameraEntity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, CameraComponent>(EntityMetaComponent("Camera"), transC1, cameraC1);
			m_engine->setCameraEntity(m_cameraEntity);
		}

		// player character
		{
			m_cesiumManAsset = AssetManager::get()->getAsset<MeshAsset>(SID("meshes/character.mesh"));
			m_skeleton = AssetManager::get()->getAsset<SkeletonAsset>(SID("meshes/character.skel"));

			m_customAnimGraph = setupAnimationGraph();

			Transform transform{};
			transform.m_rotation = glm::quat(glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f));
			transform.m_scale = glm::vec3(0.01f);
			transform.m_translation.y = 4.0f;
			TransformComponent transC(transform, Mobility::Dynamic);

			SkinnedMeshComponent meshC{};
			meshC.m_mesh = m_cesiumManAsset;
			meshC.m_skeleton = m_skeleton;
			meshC.m_animationGraph = m_customAnimGraph;
			meshC.m_boundingSphereSizeFactor = 100.0f;

			CharacterControllerComponent ccC{};
			ccC.m_translationHeightOffset = 0.0f;
			ccC.m_capsuleRadius = 0.3f;
			CharacterMovementComponent movC{};
			movC.m_active = true;

			m_playerEntity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, SkinnedMeshComponent, CharacterControllerComponent, CharacterMovementComponent>(EntityMetaComponent("Player"), transC, meshC, ccC, movC);
		}

		// ground plane
		{
			TransformComponent transC{};

			PhysicsComponent physicsC{};
			physicsC.m_mobility = PhysicsMobility::STATIC;
			physicsC.m_physicsShapeType = PhysicsShapeType::PLANE;
			//physicsC.m_materialHandle = m_physicsMaterial;

			auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, PhysicsComponent>(EntityMetaComponent("Ground Plane"), transC, physicsC);
		}

		// sponza
		EntityID sponzaEntity = {};
		{
			m_sponzaAsset = AssetManager::get()->getAsset<MeshAsset>(SID("meshes/sponza.mesh"));

			Transform transform{};
			transform.m_translation = glm::vec3(0.65f, 0.0f, 0.35f);
			TransformComponent transC(transform);

			MeshComponent meshC{ m_sponzaAsset };

			PhysicsComponent physicsC{};
			physicsC.m_mobility = PhysicsMobility::STATIC;
			physicsC.m_physicsShapeType = PhysicsShapeType::TRIANGLE_MESH;
			physicsC.m_physicsMesh = m_sponzaAsset;
			//physicsC.m_triangleMeshHandle = m_sponzaAsset->getPhysicsTriangleMeshhandle();
			//physicsC.m_materialHandle = m_physicsMaterial;

			sponzaEntity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, MeshComponent, PhysicsComponent>(EntityMetaComponent("Sponza"), transC, meshC, physicsC);
		}

		//// cesium man
		//{
		//	m_cesiumManAsset = AssetManager::get()->getAsset<MeshAsset>(SID("meshes/character"));
		//	m_skeleton = AssetManager::get()->getAsset<SkeletonAsset>(SID("meshes/character.skel"));
		//	m_idleClip = AssetManager::get()->getAsset<AnimationClipAsset>(SID("meshes/idle.anim"));
		//	m_walkClip = AssetManager::get()->getAsset<AnimationClipAsset>(SID("meshes/walking.anim"));
		//	m_runClip = AssetManager::get()->getAsset<AnimationClipAsset>(SID("meshes/running0.anim"));
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
			m_meshAsset = AssetManager::get()->getAsset<MeshAsset>(SID("meshes/uvsphere.mesh"));

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
			Transform transform{};
			transform.m_rotation = glm::quat(glm::vec3(glm::radians(-18.0f), 0.0f, 0.0f));
			TransformComponent transC(transform);

			LightComponent lightC{};
			lightC.m_type = LightComponent::Type::Directional;
			lightC.m_shadows = true;
			lightC.m_cascadeCount = 4;
			lightC.m_intensity = 10.0f;

			auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, LightComponent>(EntityMetaComponent("Directional Light"), transC, lightC);
			TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
		}

		// reflection probes
		{
			// center
			{
				Transform transform{};
				transform.m_translation = glm::vec3(0.0f, 6.475f, 0.0f);
				transform.m_scale = glm::vec3(10.0f, 6.525f, 2.4f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_captureOffset = glm::vec3(0.0f, -4.475, 0.0f);
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.5f;
				probeC.m_boxFadeDistances[5] = 0.5f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Center"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}

			// lower halls
			{
				Transform transform{};
				transform.m_translation = glm::vec3(0.0f, 1.925f, 4.26f);
				transform.m_scale = glm::vec3(10.0f, 1.975f, 2.0f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.0f;
				probeC.m_boxFadeDistances[5] = 0.2f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Lower Halls 0"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}
			{
				Transform transform{};
				transform.m_translation = glm::vec3(0.0f, 1.925f, -4.22f);
				transform.m_scale = glm::vec3(10.0f, 1.975f, 1.95f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.2f;
				probeC.m_boxFadeDistances[5] = 0.0f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Lower Halls 1"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}

			// lower ends
			{
				Transform transform{};
				transform.m_translation = glm::vec3(-11.330, 1.925f, 0.0f);
				transform.m_scale = glm::vec3(2.35f, 1.975f, 6.15f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.0f;
				probeC.m_boxFadeDistances[4] = 0.0f;
				probeC.m_boxFadeDistances[5] = 0.0f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Lower Ends 0"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}
			{
				Transform transform{};
				transform.m_translation = glm::vec3(11.375f, 1.925f, 0.0f);
				transform.m_scale = glm::vec3(2.325f, 1.975f, 6.15f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_boxFadeDistances[0] = 0.0f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.0f;
				probeC.m_boxFadeDistances[5] = 0.0f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Lower Ends 1"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}


			// upper halls
			{
				Transform transform{};
				transform.m_translation = glm::vec3(0.0f, 6.415f, 4.23f);
				transform.m_scale = glm::vec3(10.3f, 2.275f, 1.9f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_captureOffset = glm::vec3(0.0f, -1.5f, 0.0f);
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.0f;
				probeC.m_boxFadeDistances[5] = 0.2f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Upper Halls 0"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}
			{
				Transform transform{};
				transform.m_translation = glm::vec3(0.0f, 6.415f, -4.210f);
				transform.m_scale = glm::vec3(10.3f, 2.275f, 1.9f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_captureOffset = glm::vec3(0.0f, -1.5f, 0.0f);
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.2f;
				probeC.m_boxFadeDistances[5] = 0.0f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Upper Halls 1"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}

			// upper ends
			{
				Transform transform{};
				transform.m_translation = glm::vec3(-11.38f, 6.415f, 0.0f);
				transform.m_scale = glm::vec3(2.3f, 2.275f, 6.125f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_captureOffset = glm::vec3(0.0f, -1.5f, 0.0f);
				probeC.m_boxFadeDistances[0] = 0.5f;
				probeC.m_boxFadeDistances[1] = 0.0f;
				probeC.m_boxFadeDistances[4] = 0.0f;
				probeC.m_boxFadeDistances[5] = 0.0f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Upper Ends 0"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}
			{
				Transform transform{};
				transform.m_translation = glm::vec3(11.495f, 6.415f, 0.0f);
				transform.m_scale = glm::vec3(2.3f, 2.275f, 6.125f);
				TransformComponent transC(transform);

				ReflectionProbeComponent probeC{};
				probeC.m_captureOffset = glm::vec3(0.0f, -1.5f, 0.0f);
				probeC.m_boxFadeDistances[0] = 0.0f;
				probeC.m_boxFadeDistances[1] = 0.5f;
				probeC.m_boxFadeDistances[4] = 0.0f;
				probeC.m_boxFadeDistances[5] = 0.0f;

				auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, ReflectionProbeComponent>(EntityMetaComponent("Reflection Probe Upper Ends 1"), transC, probeC);
				TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
			}
		}

		// irradiance volume
		{
			IrradianceVolumeComponent volumeC{};
			volumeC.m_resolutionX = 15;
			volumeC.m_resolutionY = 6;
			volumeC.m_resolutionZ = 8;
			volumeC.m_fadeoutStart = 2.5f;
			volumeC.m_fadeoutEnd = 3.0f;

			float spacing = 2.0f;

			Transform transform{};
			transform.m_translation = glm::vec3(-15.0f, 0.0f, -8.0f) + glm::vec3(volumeC.m_resolutionX, volumeC.m_resolutionY, volumeC.m_resolutionZ) * spacing * 0.5f;
			transform.m_scale = glm::vec3(volumeC.m_resolutionX, volumeC.m_resolutionY, volumeC.m_resolutionZ) * spacing * 0.5f;
			TransformComponent transC(transform);
			
			auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, IrradianceVolumeComponent>(EntityMetaComponent("Irradiance Volume"), transC, volumeC);
			TransformHierarchy::attach(m_engine->getECS(), entity, sponzaEntity, true);
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

			Camera camera = CameraECSAdapter::createFromComponents(tc, cc);

			auto *playerMovementComponent = m_engine->getECS()->getComponent<CharacterMovementComponent>(m_playerEntity);
			m_thirdPersonCameraController->update(deltaTime, camera, m_engine->getECS()->getComponent<TransformComponent>(m_playerEntity), playerMovementComponent, m_engine->getPhysics());
			CameraECSAdapter::updateComponents(camera, m_engine->getECS(), m_cameraEntity);

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
		//for (auto n : m_engine->getLevel()->getSceneGraphNodes())
		//{
		//	m_engine->getECS()->destroyEntity(n->m_entity);
		//}

		m_engine->getECS()->clear();

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
	Asset<MeshAsset> m_meshAsset;
	Asset<MeshAsset> m_sponzaAsset;
	Asset<MeshAsset> m_cesiumManAsset;
	Asset<SkeletonAsset> m_skeleton;
	EntityID m_cameraEntity;
	EntityID m_manEntity;
	EntityID m_playerEntity;
	PhysicsMaterialHandle m_physicsMaterial = {};
	AnimationGraph *m_customAnimGraph = {};
	bool m_playing = true;

	void createPhysicsObject(const glm::vec3 &pos, const glm::vec3 &vel, PhysicsMobility mobility)
	{
		Transform transform{};
		transform.m_translation.x = pos.x;
		transform.m_translation.y = pos.y;
		transform.m_translation.z = pos.z;
		transform.m_scale = glm::vec3(0.25f);
		TransformComponent transC(transform, Mobility::Dynamic);

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
		//physicsC.m_materialHandle = m_physicsMaterial;

		auto entity = m_engine->getECS()->createEntity<EntityMetaComponent, TransformComponent, MeshComponent, PhysicsComponent>(EntityMetaComponent("Physics Sphere"), transC, meshC, physicsC);
	}
};

int main(int argc, char *argv[])
{
	DummyGameLogic gameLogic;
	Editor editor(&gameLogic);
	Engine engine;
	return engine.start(argc, argv, &editor);
}