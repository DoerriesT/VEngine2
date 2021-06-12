#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include "reflection/TypeInfo.h"

class Reflection;

struct TransformComponent
{
	enum class Mobility
	{
		STATIC, DYNAMIC
	};

	Mobility m_mobility = Mobility::DYNAMIC;
	glm::vec3 m_translation = glm::vec3(0.0f);
	glm::quat m_rotation = glm::quat();
	glm::vec3 m_scale = glm::vec3(1.0f);
	glm::mat4 m_transformation;
	glm::mat4 m_previousTransformation;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(TransformComponent, "ABA16580-3F7A-404A-BE66-77353FD6E550")