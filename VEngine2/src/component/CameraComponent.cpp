#include "CameraComponent.h"
#include <glm/trigonometric.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void CameraComponent::onGUI(void *instance) noexcept
{
	CameraComponent &cc = *reinterpret_cast<CameraComponent *>(instance);

	float degrees = glm::degrees(cc.m_fovy);
	ImGui::DragFloat("Field of View", &degrees, 1.0f, 1.0f, 179.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	cc.m_fovy = glm::radians(degrees);
	ImGuiHelpers::Tooltip("The vertical field of view.");

	ImGui::DragFloat("Near Plane", &cc.m_near, 0.05f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the camera near plane.");

	ImGui::DragFloat("Far Plane", &cc.m_far, 0.1f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the camera far plane.");
}
