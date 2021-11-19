#include "OutlineComponent.h"
#include "graphics/imgui/imgui.h"

void OutlineComponent::onGUI(void *instance) noexcept
{
	OutlineComponent &c = *reinterpret_cast<OutlineComponent *>(instance);
	ImGui::Checkbox("Outlined", &c.m_outlined);
}

void OutlineComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void OutlineComponent::fromLua(lua_State *L, void *instance) noexcept
{
}

void EditorOutlineComponent::onGUI(void *instance) noexcept
{
	EditorOutlineComponent &c = *reinterpret_cast<EditorOutlineComponent *>(instance);
	ImGui::Checkbox("Outlined", &c.m_outlined);
}

void EditorOutlineComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void EditorOutlineComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
