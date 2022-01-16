#include "OutlineComponent.h"
#include "graphics/imgui/imgui.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(OutlineComponent &c, Stream &stream) noexcept
{
	serializeBool(stream, c.m_outlined);

	return true;
}

void OutlineComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	OutlineComponent &c = *reinterpret_cast<OutlineComponent *>(instance);
	ImGui::Checkbox("Outlined", &c.m_outlined);
}

bool OutlineComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<OutlineComponent *>(instance), stream);
}

bool OutlineComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<OutlineComponent *>(instance), stream);
}

void OutlineComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void OutlineComponent::fromLua(lua_State *L, void *instance) noexcept
{
}

void EditorOutlineComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	EditorOutlineComponent &c = *reinterpret_cast<EditorOutlineComponent *>(instance);
	ImGui::Checkbox("Outlined", &c.m_outlined);
}

bool EditorOutlineComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return false;
}

bool EditorOutlineComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return false;
}

void EditorOutlineComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void EditorOutlineComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
