#pragma once
#include <stdint.h>

struct lua_State;

struct LightComponent
{
	uint32_t m_color = 0xFFFFFFFF;
	float m_luminousPower = 1000.0f;
	float m_radius = 8.0f;
	float m_outerAngle = 0.785f;
	float m_innerAngle = 0.26f;
	bool m_shadows = false;

	static void onGUI(void *instance) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "LightComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Light Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};