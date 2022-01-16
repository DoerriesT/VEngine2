#pragma once
struct lua_State;
struct TransformComponent;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct OutlineComponent
{
	bool m_outlined;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "OutlineComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Outline Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};

struct EditorOutlineComponent
{
	bool m_outlined;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "EditorOutlineComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Editor Outline Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};