#pragma once
#include "asset/ScriptAsset.h"
#include "ecs/ECSCommon.h"

struct lua_State;
struct TransformComponent;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct ScriptComponent
{
	Asset<ScriptAssetData> m_script;
	lua_State *m_L = nullptr;

	ScriptComponent() = default;
	ScriptComponent(const ScriptComponent &other) noexcept;
	ScriptComponent(ScriptComponent &&other) noexcept;
	ScriptComponent &operator=(const ScriptComponent &other) noexcept;
	ScriptComponent &operator=(ScriptComponent &&other) noexcept;
	~ScriptComponent() noexcept;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "ScriptComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Script Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};