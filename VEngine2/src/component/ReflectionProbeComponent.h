#pragma once
#include <glm/vec3.hpp>

struct lua_State;
struct TransformComponent;
class Renderer;

struct ReflectionProbeComponent
{
	glm::vec3 m_captureOffset = glm::vec3(0.0f);
	float m_nearPlane = 0.5f;
	float m_farPlane = 50.0f;
	float m_boxFadeDistances[6] = {}; // x, -x, y, -y, z, -z
	bool m_lockedFadeDistance = false;
	bool m_recapture = false;

	uint32_t m_cacheSlot = UINT32_MAX; // internal use
	uint32_t m_lastLit = UINT32_MAX; // internal use
	bool m_rendered = false; // internal use

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "ReflectionProbeComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Reflection Probe Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};