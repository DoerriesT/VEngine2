#pragma once
struct lua_State;
struct TransformComponent;
class Renderer;

struct OutlineComponent
{
	bool m_outlined;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
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
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "EditorOutlineComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Editor Outline Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};