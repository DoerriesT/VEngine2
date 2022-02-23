#include "OutlineComponent.h"
#include "graphics/imgui/imgui.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(OutlineComponent &c, Stream &stream) noexcept
{
	serializeBool(stream, c.m_outlined);

	return true;
}

void OutlineComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	OutlineComponent &c = *reinterpret_cast<OutlineComponent *>(instance);
	ImGui::Checkbox("Outlined", &c.m_outlined);
}

bool OutlineComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<OutlineComponent *>(instance), stream);
}

bool OutlineComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<OutlineComponent *>(instance), stream);
}

void OutlineComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}

void OutlineComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}

void EditorOutlineComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	EditorOutlineComponent &c = *reinterpret_cast<EditorOutlineComponent *>(instance);
	ImGui::Checkbox("Outlined", &c.m_outlined);
}

bool EditorOutlineComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return false;
}

bool EditorOutlineComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return false;
}

void EditorOutlineComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}

void EditorOutlineComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}
