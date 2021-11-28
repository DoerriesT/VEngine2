#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct lua_State;

struct Transform
{
	glm::vec3 m_translation = glm::vec3(0.0f);
	glm::quat m_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 m_scale = glm::vec3(1.0f);
};

struct TransformComponent
{
	enum class Mobility
	{
		STATIC, DYNAMIC
	};

	Mobility m_mobility = Mobility::DYNAMIC;
	Transform m_transform;
	Transform m_prevTransform;

	static void onGUI(void *instance) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "TransformComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Transform Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};