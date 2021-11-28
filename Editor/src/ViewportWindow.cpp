#include "ViewportWindow.h"
#include <graphics/imgui/imgui.h>
#include <Engine.h>
#include <graphics/Renderer.h>
#include <math.h>
#include <component/CameraComponent.h>
#include <component/TransformComponent.h>
#include <component/CharacterControllerComponent.h>
#include <input/FlyCameraController.h>
#include <graphics/Camera.h>

ViewportWindow::ViewportWindow(Engine *engine, const EntityID *editorCameraEntity) noexcept
	:m_engine(engine),
	m_editorCameraEntityPtr(editorCameraEntity),
	m_flyCameraController(new FlyCameraController(engine->getECS()))
{
}

ViewportWindow::~ViewportWindow() noexcept
{
	delete m_flyCameraController;
}

void ViewportWindow::draw(float deltaTime, bool gameIsPlaying) noexcept
{
	if (!m_visible)
	{
		return;
	}

	Renderer *renderer = m_engine->getRenderer();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::Begin("Viewport");

	auto viewportOffset = ImGui::GetWindowContentRegionMin();
	auto windowPos = ImGui::GetWindowPos();
	viewportOffset.x += windowPos.x;
	viewportOffset.y += windowPos.y;
	auto viewportSize = ImGui::GetContentRegionAvail();

	ImGui::Image(renderer->getEditorViewportTextureID(), viewportSize);

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	m_engine->setEditorViewport((int32_t)viewportOffset.x, (int32_t)viewportOffset.y, (uint32_t)fmaxf(viewportSize.x, 0.0f), (uint32_t)fmaxf(viewportSize.y, 0.0f));

	auto mousePos = ImGui::GetMousePos();
	m_engine->setPickingPos(static_cast<uint32_t>(mousePos.x - viewportOffset.x), static_cast<uint32_t>(mousePos.y - viewportOffset.y));

	if (!gameIsPlaying)
	{
		TransformComponent *tc = m_engine->getECS()->getComponent<TransformComponent>(*m_editorCameraEntityPtr);
		CameraComponent *cc = m_engine->getECS()->getComponent<CameraComponent>(*m_editorCameraEntityPtr);
		assert(tc && cc);

		// make sure aspect ratio of editor camera is correct
		cc->m_aspectRatio = (viewportSize.x > 0 && viewportSize.y > 0) ? viewportSize.x / (float)viewportSize.y : 1.0f;

		Camera camera(*tc, *cc);

		m_flyCameraController->update(deltaTime, camera);
	}
}

void ViewportWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool ViewportWindow::isVisible() const noexcept
{
	return m_visible;
}
