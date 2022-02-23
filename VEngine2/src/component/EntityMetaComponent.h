#pragma once
#include "UUID.h"
#include "ecs/ECSCommon.h"

class ECS;
struct lua_State;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;
struct TransformComponent;

struct EntityMetaComponent
{
	static constexpr size_t k_maxNameLength = 256;
	char m_name[k_maxNameLength] = {};
	TUUID m_uuid = TUUID::createRandom();

	EntityMetaComponent() noexcept = default;
	explicit EntityMetaComponent(const char *name) noexcept;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "EntityMetaComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Entity Meta Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};