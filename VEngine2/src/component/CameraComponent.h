#pragma once
#include <glm/mat4x4.hpp>
#include "reflection/TypeInfo.h"

class Reflection;

struct CameraComponent
{
	float m_aspectRatio = 1.0f;
	float m_fovy = 1.57079632679f; // 90 degrees
	float m_near = 0.1f;
	float m_far = 300.0f;
	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(CameraComponent, "2F315E1B-3277-4A40-8BBC-A5BEEB290A7B")