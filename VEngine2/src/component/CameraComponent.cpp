#include "CameraComponent.h"
#include <glm/trigonometric.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"

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

void CameraComponent::toLua(lua_State *L, void *instance) noexcept
{
	CameraComponent &c = *reinterpret_cast<CameraComponent *>(instance);

	LuaUtil::setTableNumberField(L, "m_aspectRatio", (lua_Number)c.m_aspectRatio);
	LuaUtil::setTableNumberField(L, "m_fovy", (lua_Number)c.m_fovy);
	LuaUtil::setTableNumberField(L, "m_near", (lua_Number)c.m_near);
	LuaUtil::setTableNumberField(L, "m_far", (lua_Number)c.m_far);
	LuaUtil::setTableNumberArrayField(L, "m_viewMatrix", &c.m_viewMatrix[0][0], 16);
	LuaUtil::setTableNumberArrayField(L, "m_projectionMatrix", &c.m_projectionMatrix[0][0], 16);
}

void CameraComponent::fromLua(lua_State *L, void *instance) noexcept
{
	CameraComponent &c = *reinterpret_cast<CameraComponent *>(instance);

	c.m_aspectRatio = (float)LuaUtil::getTableNumberField(L, "m_aspectRatio");
	c.m_fovy = (float)LuaUtil::getTableNumberField(L, "m_fovy");
	c.m_near = (float)LuaUtil::getTableNumberField(L, "m_near");
	c.m_far = (float)LuaUtil::getTableNumberField(L, "m_far");
	LuaUtil::getTableNumberArrayField(L, "m_viewMatrix", 16, &c.m_viewMatrix[0][0]);
	LuaUtil::getTableNumberArrayField(L, "m_projectionMatrix", 16, &c.m_projectionMatrix[0][0]);
}
