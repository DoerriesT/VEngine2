#include "LightComponent.h"
#include <glm/trigonometric.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void LightComponent::onGUI(void *instance) noexcept
{
	LightComponent &c = *reinterpret_cast<LightComponent *>(instance);

	ImGuiHelpers::ColorEdit3("Color", &c.m_color);
	ImGuiHelpers::Tooltip("The sRGB color of the light source.");

	ImGui::DragFloat("Intensity", &c.m_luminousPower, 1.0f, 0.0f, FLT_MAX);
	ImGuiHelpers::Tooltip("Intensity as luminous power (in lumens).");

	ImGui::DragFloat("Radius", &c.m_radius, 1.0f, 0.01f, FLT_MAX);
	ImGuiHelpers::Tooltip("The radius of the sphere of influence of the light source.");

	float outerAngleDegrees = glm::degrees(c.m_outerAngle);
	ImGui::DragFloat("Outer Spot Angle", &outerAngleDegrees, 1.0f, 1.0f, 179.0f);
	c.m_outerAngle = glm::radians(outerAngleDegrees);
	ImGuiHelpers::Tooltip("The outer angle of the spot light cone.");

	float innerAngleDegrees = glm::degrees(c.m_innerAngle);
	ImGui::DragFloat("Inner Spot Angle", &innerAngleDegrees, 1.0f, 1.0f, outerAngleDegrees);
	c.m_innerAngle = glm::radians(innerAngleDegrees);
	ImGuiHelpers::Tooltip("The inner angle of the spot light cone.");

	ImGui::Checkbox("Casts Shadows", &c.m_shadows);
	ImGuiHelpers::Tooltip("Enables shadows for this light source.");
}

void LightComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void LightComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
