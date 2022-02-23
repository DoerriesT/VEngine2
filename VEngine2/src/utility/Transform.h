#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

enum class Mobility
{
	Static, Dynamic
};

struct Transform
{
	glm::vec3 m_translation = glm::vec3(0.0f);
	glm::quat m_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 m_scale = glm::vec3(1.0f);

	static Transform updateFromParent(const Transform &local, const Transform &parent) noexcept
	{
		Transform result;

		result.m_rotation = glm::normalize(parent.m_rotation * local.m_rotation);
		result.m_scale = parent.m_scale * local.m_scale;
		result.m_translation = parent.m_translation + (parent.m_rotation * (parent.m_scale * local.m_translation));

		return result;
	}

	static Transform makeRelative(const Transform &childGlobal, const Transform &parentGlobal) noexcept
	{
		Transform result;

		result.m_scale = childGlobal.m_scale / parentGlobal.m_scale;
		result.m_rotation = glm::normalize(childGlobal.m_rotation * glm::inverse(parentGlobal.m_rotation));
		result.m_translation = glm::inverse(parentGlobal.m_rotation) * ((childGlobal.m_translation - parentGlobal.m_translation) / parentGlobal.m_scale);

		return result;
	}

	static glm::vec3 apply(const Transform &transform, const glm::vec3 &position) noexcept
	{
		return transform.m_translation + (transform.m_rotation * (transform.m_scale * position));
	}
};