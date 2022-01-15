#include "TransformComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"

void TransformComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	int mobilityInt = static_cast<int>(c.m_mobility);
	ImGui::RadioButton("Static", &mobilityInt, static_cast<int>(Mobility::STATIC)); ImGui::SameLine();
	ImGui::RadioButton("Dynamic", &mobilityInt, static_cast<int>(Mobility::DYNAMIC));
	c.m_mobility = static_cast<Mobility>(mobilityInt);

	ImGui::DragFloat3("Translation", &c.m_transform.m_translation.x, 0.1f);

	{
		glm::vec3 eulerAnglesBefore = glm::degrees(glm::eulerAngles(c.m_transform.m_rotation));
		glm::vec3 eulerAngles = eulerAnglesBefore;
		if (ImGui::DragFloat3("Rotation", &eulerAngles.x, 1.0f, -360.0f, 360.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			glm::vec3 diff = eulerAngles - eulerAnglesBefore;
			glm::quat diffQuat = glm::quat(glm::radians(diff));

			c.m_transform.m_rotation = glm::normalize(c.m_transform.m_rotation * diffQuat);
		}
	}

	ImGui::DragFloat3("Scale", &c.m_transform.m_scale.x, 0.1f);
	
}

void TransformComponent::toLua(lua_State *L, void *instance) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	LuaUtil::setTableIntegerField(L, "m_mobility", (lua_Integer)c.m_mobility);
	LuaUtil::setTableNumberField(L, "m_translationX", (lua_Number)c.m_transform.m_translation.x);
	LuaUtil::setTableNumberField(L, "m_translationY", (lua_Number)c.m_transform.m_translation.y);
	LuaUtil::setTableNumberField(L, "m_translationZ", (lua_Number)c.m_transform.m_translation.z);
	LuaUtil::setTableNumberField(L, "m_rotationX", (lua_Number)c.m_transform.m_rotation.x);
	LuaUtil::setTableNumberField(L, "m_rotationY", (lua_Number)c.m_transform.m_rotation.y);
	LuaUtil::setTableNumberField(L, "m_rotationZ", (lua_Number)c.m_transform.m_rotation.z);
	LuaUtil::setTableNumberField(L, "m_rotationW", (lua_Number)c.m_transform.m_rotation.w);
	LuaUtil::setTableNumberField(L, "m_scaleX", (lua_Number)c.m_transform.m_scale.x);
	LuaUtil::setTableNumberField(L, "m_scaleY", (lua_Number)c.m_transform.m_scale.y);
	LuaUtil::setTableNumberField(L, "m_scaleZ", (lua_Number)c.m_transform.m_scale.z);
}

void TransformComponent::fromLua(lua_State *L, void *instance) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	c.m_mobility = (TransformComponent::Mobility)LuaUtil::getTableIntegerField(L, "m_mobility");
	c.m_transform.m_translation.x = (float)LuaUtil::getTableNumberField(L, "m_translationX");
	c.m_transform.m_translation.y = (float)LuaUtil::getTableNumberField(L, "m_translationY");
	c.m_transform.m_translation.z = (float)LuaUtil::getTableNumberField(L, "m_translationZ");
	c.m_transform.m_rotation.x = (float)LuaUtil::getTableNumberField(L, "m_rotationX");
	c.m_transform.m_rotation.y = (float)LuaUtil::getTableNumberField(L, "m_rotationY");
	c.m_transform.m_rotation.z = (float)LuaUtil::getTableNumberField(L, "m_rotationZ");
	c.m_transform.m_rotation.w = (float)LuaUtil::getTableNumberField(L, "m_rotationW");
	c.m_transform.m_scale.x = (float)LuaUtil::getTableNumberField(L, "m_scaleX");
	c.m_transform.m_scale.y = (float)LuaUtil::getTableNumberField(L, "m_scaleY");
	c.m_transform.m_scale.z = (float)LuaUtil::getTableNumberField(L, "m_scaleZ");
}
