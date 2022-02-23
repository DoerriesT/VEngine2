#include "ViewportWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/ImGuizmo.h>
#include <Engine.h>
#include <graphics/Renderer.h>
#include <math.h>
#include <component/CameraComponent.h>
#include <component/TransformComponent.h>
#include <component/CharacterControllerComponent.h>
#include <component/CameraComponent.h>
#include <input/FlyCameraController.h>
#include <graphics/Camera.h>
#include <glm/gtx/transform.hpp>
#include <utility/Transform.h>
#include <TransformHierarchy.h>

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

void ViewportWindow::draw(float deltaTime, bool gameIsPlaying, EntityID selectedEntity) noexcept
{
	if (!m_visible)
	{
		return;
	}

	Renderer *renderer = m_engine->getRenderer();

	ImVec2 viewportOffset = {};
	ImVec2 viewportSize = {};
	m_hasFocus = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::Begin("Viewport");
	{
		m_hasFocus = ImGui::IsWindowFocused();

		viewportOffset = ImGui::GetWindowContentRegionMin();
		auto windowPos = ImGui::GetWindowPos();
		viewportOffset.x += windowPos.x;
		viewportOffset.y += windowPos.y;
		viewportSize = ImGui::GetContentRegionAvail();

		// texture with rendered scene
		ImGui::Image(renderer->getEditorViewportTextureID(), viewportSize);

		// update editor camera
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

		// render gizmo
		if (selectedEntity != k_nullEntity)
		{
			ECS &ecs = *m_engine->getECS();
			if (auto *tc = ecs.getComponent<TransformComponent>(selectedEntity); tc != nullptr)
			{
				auto cameraEntity = m_engine->getCameraEntity();
				if (ecs.hasComponents<TransformComponent, CameraComponent>(cameraEntity))
				{
					ImGuizmo::OPERATION op = ImGuizmo::OPERATION::TRANSLATE;
					switch (m_gizmoType)
					{
					case 0: op = ImGuizmo::OPERATION::TRANSLATE; break;
					case 1: op = ImGuizmo::OPERATION::ROTATE; break;
					case 2: op = ImGuizmo::OPERATION::SCALE; break;
					default:
						break;
					}

					// compute matrices
					auto *camC = ecs.getComponent<CameraComponent>(cameraEntity);
					Camera camera(*ecs.getComponent<TransformComponent>(cameraEntity), *camC);
					auto viewMatrix = camera.getViewMatrix();
					auto projMatrix = glm::perspective(camC->m_fovy, camC->m_aspectRatio, camC->m_near, camC->m_far);

					ImGuizmo::SetOrthographic(false);
					ImGuizmo::SetDrawlist();
					ImGuizmo::SetRect(viewportOffset.x, viewportOffset.y, viewportSize.x, viewportSize.y);

					glm::mat4 transform = glm::translate(tc->m_globalTransform.m_translation) * glm::mat4_cast(tc->m_globalTransform.m_rotation) * glm::scale(tc->m_globalTransform.m_scale);

					float snapVar = m_gizmoType == 0 ? m_translateSnap : m_gizmoType == 1 ? m_rotateSnap : m_scaleSnap;
					float snapArr[] = { snapVar, snapVar, snapVar };
					ImGuizmo::Manipulate(&viewMatrix[0][0], &projMatrix[0][0], op, m_gizmoLocal ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD, &transform[0][0], nullptr, snapArr);

					glm::vec3 position;
					glm::vec3 eulerAngles;
					glm::vec3 scale;
					ImGuizmo::DecomposeMatrixToComponents((float *)&transform, (float *)&position, (float *)&eulerAngles, (float *)&scale);

					Transform newTransform = tc->m_globalTransform;

					switch (op)
					{
					case ImGuizmo::TRANSLATE:
						newTransform.m_translation = position;
						break;
					case ImGuizmo::ROTATE:
						newTransform.m_rotation = glm::quat(glm::radians(eulerAngles));
						break;
					case ImGuizmo::SCALE:
						newTransform.m_scale = scale;
						break;
					case ImGuizmo::BOUNDS:
						break;
					default:
						break;
					}

					TransformHierarchy::setGlobalTransform(&ecs, selectedEntity, newTransform, tc);
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	// draw overlay
	ImGui::SetNextWindowPos(viewportOffset);
	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	ImGuiWindowFlags overlayflags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
	if (ImGui::Begin("Viewport Overlay", nullptr, overlayflags))
	{
		ImGui::RadioButton("Translate", &m_gizmoType, 0); ImGui::SameLine();
		ImGui::RadioButton("Rotate", &m_gizmoType, 1); ImGui::SameLine();
		ImGui::RadioButton("Scale", &m_gizmoType, 2); ImGui::SameLine();

		ImGui::Spacing(); ImGui::SameLine();

		ImGui::Checkbox("Local", &m_gizmoLocal); ImGui::SameLine();

		ImGui::Spacing(); ImGui::SameLine();

		float *snapVar = m_gizmoType == 0 ? &m_translateSnap : m_gizmoType == 1 ? &m_rotateSnap : &m_scaleSnap;
		ImGui::DragFloat("Snap", snapVar, 0.01f, 0.0f, FLT_MAX);

		ImGui::SameLine();
		bool gridRenderingEnabled = m_engine->getRenderer()->isGridRenderingEnabled();
		if (ImGui::Checkbox("Grid", &gridRenderingEnabled))
		{
			m_engine->getRenderer()->enableGridRendering(gridRenderingEnabled);
		}

		m_hasFocus = m_hasFocus && !ImGui::IsWindowFocused();
	}
	ImGui::End();

	

	// tell engine about viewport size and picking pos
	m_engine->setEditorViewport((int32_t)viewportOffset.x, (int32_t)viewportOffset.y, (uint32_t)fmaxf(viewportSize.x, 0.0f), (uint32_t)fmaxf(viewportSize.y, 0.0f));
	auto mousePos = ImGui::GetMousePos();
	m_engine->setPickingPos(static_cast<uint32_t>(mousePos.x - viewportOffset.x), static_cast<uint32_t>(mousePos.y - viewportOffset.y));
}

void ViewportWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool ViewportWindow::isVisible() const noexcept
{
	return m_visible;
}

bool ViewportWindow::hasFocus() const noexcept
{
	return m_hasFocus;
}
