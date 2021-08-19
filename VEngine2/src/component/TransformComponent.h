#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct TransformComponent
{
	enum class Mobility
	{
		STATIC, DYNAMIC
	};

	Mobility m_mobility = Mobility::DYNAMIC;
	glm::vec3 m_translation = glm::vec3(0.0f);
	glm::quat m_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 m_scale = glm::vec3(1.0f);
	glm::mat4 m_transformation;
	glm::mat4 m_previousTransformation;

	static void onGUI(void *instance) noexcept;
	static const char *getComponentName() noexcept { return "TransformComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Transform Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};