#include "EntityMetaComponent.h"
#include "script/LuaUtil.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(EntityMetaComponent &c, Stream &stream) noexcept
{
	serializeBytes(stream, EntityMetaComponent::k_maxNameLength, c.m_name);
	serializeBytes(stream, 16, reinterpret_cast<char *>(c.m_uuid.m_data));

	return true;
}

EntityMetaComponent::EntityMetaComponent(const char *name) noexcept
{
	strcpy_s(m_name, name);
}

void EntityMetaComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
}

bool EntityMetaComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<EntityMetaComponent *>(instance), stream);
}

bool EntityMetaComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<EntityMetaComponent *>(instance), stream);
}

void EntityMetaComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}

void EntityMetaComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}
