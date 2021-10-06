#pragma once
#include <glm/mat4x4.hpp>

struct lua_State;

struct CameraComponent
{
	float m_aspectRatio = 1.0f;
	float m_fovy = 1.57079632679f; // 90 degrees
	float m_near = 0.1f;
	float m_far = 300.0f;
	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;

	static void onGUI(void *instance) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "CameraComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Camera Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};