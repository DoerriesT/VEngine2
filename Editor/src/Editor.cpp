#include "Editor.h"
#include <Engine.h>
#include <graphics/imgui/imgui.h>
#include <graphics/Renderer.h>
#include "ViewportWindow.h"
#include "InspectorWindow.h"
#include "AssetBrowserWindow.h"
#include "SceneGraphWindow.h"
#include <component/CameraComponent.h>
#include <component/TransformComponent.h>
#include <component/CharacterControllerComponent.h>
#include <component/PlayerMovementComponent.h>

Editor::Editor(IGameLogic *gameLogic) noexcept
	:m_gameLogic(gameLogic)
{
}

void Editor::init(Engine *engine) noexcept
{
	m_engine = engine;

	TransformComponent transC{};
	transC.m_translation = glm::vec3(0.0f, 2.0f, 12.0f);
	CameraComponent cameraC{};
	cameraC.m_fovy = glm::radians(60.0f);
	CharacterControllerComponent ccC{};
	PlayerMovementComponent pmC{};
	pmC.m_active = true;

	m_editorCameraEntity = m_engine->getECS()->createEntity<TransformComponent, CameraComponent, CharacterControllerComponent, PlayerMovementComponent>(transC, cameraC, ccC, pmC);

	m_engine->setEditorMode(true);
	m_viewportWindow = new ViewportWindow(engine, &m_editorCameraEntity);
	m_inspectorWindow = new InspectorWindow(engine);
	m_assetBrowserWindow = new AssetBrowserWindow(engine);
	m_sceneGraphWindow = new SceneGraphWindow(engine);

	m_gameLogic->init(engine);
}

void Editor::update(float deltaTime) noexcept
{
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

	m_sceneGraphWindow->draw();
	m_inspectorWindow->draw(m_sceneGraphWindow->getSelectedEntity());
	m_viewportWindow->draw(deltaTime);
	m_assetBrowserWindow->draw();
	

	m_gameLogic->update(deltaTime);

	m_engine->getRenderer()->setCameraEntity(m_editorCameraEntity);
}

void Editor::shutdown() noexcept
{
	m_gameLogic->shutdown();

	delete m_viewportWindow;
	delete m_inspectorWindow;
	delete m_assetBrowserWindow;
	delete m_sceneGraphWindow;
}
