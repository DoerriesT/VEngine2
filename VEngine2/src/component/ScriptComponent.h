#pragma once
#include "asset/ScriptAsset.h"

struct lua_State;
struct TransformComponent;
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

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "ScriptComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Script Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};