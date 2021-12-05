#include "Editor.h"
#include <Engine.h>
#include <graphics/imgui/imgui.h>
#include <graphics/Renderer.h>
#include "ViewportWindow.h"
#include "InspectorWindow.h"
#include "AssetBrowserWindow.h"
#include "SceneGraphWindow.h"
#include "AnimationGraphWindow.h"
#include <component/CameraComponent.h>
#include <component/TransformComponent.h>
#include <component/SkinnedMeshComponent.h>
#include <asset/AssetMetaDataRegistry.h>
#include <profiling/Profiling.h>

Editor::Editor(IGameLogic *gameLogic) noexcept
	:m_gameLogic(gameLogic)
{
}

void Editor::init(Engine *engine) noexcept
{
	AssetMetaDataRegistry::get()->init();
	m_engine = engine;

	TransformComponent transC{};
	transC.m_transform.m_translation = glm::vec3(-12.0f, 2.0f, 0.0f);
	transC.m_transform.m_rotation = glm::quat(glm::vec3(0.0f, glm::radians(-90.0f), 0.0f));
	CameraComponent cameraC{};
	cameraC.m_fovy = glm::radians(60.0f);

	m_editorCameraEntity = m_engine->getECS()->createEntity<TransformComponent, CameraComponent>(transC, cameraC);

	m_engine->setEditorMode(true);
	m_viewportWindow = new ViewportWindow(engine, &m_editorCameraEntity);
	m_inspectorWindow = new InspectorWindow(engine);
	m_assetBrowserWindow = new AssetBrowserWindow(engine);
	m_sceneGraphWindow = new SceneGraphWindow(engine);
	m_animationGraphWindow = new AnimationGraphWindow(engine);

	m_gameLogic->init(engine);
	m_gameIsPlaying = false;
	m_gameLogic->setPlaying(m_gameIsPlaying);
}

void Editor::setPlaying(bool playing) noexcept
{

}

void Editor::update(float deltaTime) noexcept
{
	PROFILING_ZONE_SCOPED;

	// begin main dock space/main menu bar
	{
		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		window_flags |= ImGuiWindowFlags_NoBackground;
		ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("MainDockSpace", nullptr, window_flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpaceID");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	}


	if (ImGui::BeginMainMenuBar())
	{
		//if (ImGui::BeginMenu("File"))
		//{
		//	// ShowExampleMenuFile();
		//	ImGui::EndMenu();
		//}
		//if (ImGui::BeginMenu("Edit"))
		//{
		//	if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
		//	if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
		//	ImGui::Separator();
		//	if (ImGui::MenuItem("Cut", "CTRL+X")) {}
		//	if (ImGui::MenuItem("Copy", "CTRL+C")) {}
		//	if (ImGui::MenuItem("Paste", "CTRL+V")) {}
		//	ImGui::EndMenu();
		//}
		if (ImGui::BeginMenu("Window"))
		{
			//ImGui::MenuItem("Entities", "", &m_showEntityWindow);
			//ImGui::MenuItem("Details", "", &m_showEntityDetailWindow);
			//ImGui::MenuItem("Profiler", "", &m_showProfilerWindow);
			//ImGui::MenuItem("Memory", "", &m_showMemoryWindow);
			//ImGui::MenuItem("Exposure Settings", "", &m_showExposureWindow);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::End();

	// quick menu
	{
		ImGui::Begin("Quick Menu", nullptr, ImGuiWindowFlags_NoDecoration);

		if (ImGui::Button(m_gameIsPlaying ? "Stop" : "Start"))
		{
			m_gameIsPlaying = !m_gameIsPlaying;
			m_gameLogic->setPlaying(m_gameIsPlaying);
		}

		ImGui::End();
	}

	m_viewportWindow->draw(deltaTime, m_gameIsPlaying, m_sceneGraphWindow->getSelectedEntity());
	m_sceneGraphWindow->draw(m_viewportWindow->hasFocus());
	m_inspectorWindow->draw(m_sceneGraphWindow->getSelectedEntity());
	m_assetBrowserWindow->draw();

	AnimationGraph *animGraph = nullptr;

	m_engine->getECS()->iterate<SkinnedMeshComponent>([&animGraph](size_t count, const EntityID *entities, SkinnedMeshComponent *skinnedMeshC)
		{
			if (count > 0)
			{
				animGraph = skinnedMeshC->m_animationGraph;
			}
		});

	m_animationGraphWindow->draw(animGraph);
	

	m_gameLogic->update(deltaTime);

	if (!m_gameIsPlaying)
	{
		m_engine->setCameraEntity(m_editorCameraEntity);
	}
}

void Editor::shutdown() noexcept
{
	m_gameLogic->shutdown();

	delete m_viewportWindow;
	delete m_inspectorWindow;
	delete m_assetBrowserWindow;
	delete m_sceneGraphWindow;
	delete m_animationGraphWindow;

	AssetMetaDataRegistry::get()->shutdown();
}
