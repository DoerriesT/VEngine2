#include "ThirdPersonCameraController.h"
#include "ecs/ECS.h"
#include "component/InputStateComponent.h"
#include "component/CharacterMovementComponent.h"
#include "component/TransformComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include "graphics/Camera.h"
#include "Log.h"
#include "physics/Physics.h"
#include <glm/gtx/transform.hpp>

ThirdPersonCameraController::ThirdPersonCameraController(ECS *ecs) noexcept
	:m_ecs(ecs)
{
}

void ThirdPersonCameraController::update(float timeDelta, Camera &camera, TransformComponent *characterTc, CharacterMovementComponent *movc, Physics *physics)
{
	if (!characterTc || !movc)
	{
		return;
	}
	const auto *inputState = m_ecs->getSingletonComponent<InputStateComponent>();

	auto sphericalToCartesian = [](float pitch, float yaw, float dist)
	{
		glm::vec3 result;
		result.x = dist * cosf(yaw) * sinf(pitch);
		result.y = dist * cosf(pitch);
		result.z = dist * sinf(yaw) * sinf(pitch);
		return result;
	};

	glm::vec3 center = characterTc->m_transform.m_translation + glm::vec3(0.0f, 2.0f, 0.0f);
	glm::vec3 cameraPos = sphericalToCartesian(m_cameraPitch, m_cameraYaw, m_prevCameraDistance);
	glm::vec3 toCamera = glm::normalize(cameraPos - center);

	m_zoomHistory = glm::mix(m_zoomHistory, inputState->m_zoomAxis, timeDelta / (timeDelta + 0.05f));
	m_cameraTargetDistance += m_zoomHistory;
	m_cameraTargetDistance = glm::clamp(m_cameraTargetDistance, 1.0f, 20.0f);

	// rotation of camera
	{
		// smooth rotation
		glm::vec2 rotateHistory = glm::mix(glm::make_vec2(m_rotateHistory), glm::vec2(inputState->m_turnRightAxis, inputState->m_lookUpAxis), timeDelta / (timeDelta + 0.05f));
		m_rotateHistory[0] = rotateHistory.x;
		m_rotateHistory[1] = rotateHistory.y;

		// apply rotation
		if ((fabsf(m_rotateHistory[0]) + fabsf(m_rotateHistory[1])) > 0.0f)
		{
			m_cameraPitch += m_rotateHistory[1];
			m_cameraYaw += m_rotateHistory[0];

			m_cameraPitch = glm::clamp(m_cameraPitch, glm::pi<float>() * 0.1f, glm::pi<float>() * 0.7f);
		}
	}

	// rotation of character
	{
		auto wrapAngle = [](float x)
		{
			float normed = x * glm::one_over_two_pi<float>();
			return glm::fract(normed) * glm::two_pi<float>();
		};

		glm::vec3 characterForward3D = glm::mat3_cast(characterTc->m_transform.m_rotation) * glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 targetDir = glm::normalize(glm::vec3(movc->m_velocityX, 0.0f, movc->m_velocityZ));
		targetDir = glm::any(glm::isnan(targetDir)) ? characterForward3D : targetDir;
		float characterYaw = atan2f(characterForward3D.z, characterForward3D.x);
		characterYaw = wrapAngle(characterYaw);
		float targetYaw = atan2f(targetDir.z, targetDir.x);
		targetYaw = wrapAngle(targetYaw);

		float angle = wrapAngle(targetYaw - characterYaw);

		// pick shortest rotation
		angle = angle >= glm::pi<float>() ? angle - glm::two_pi<float>() : angle;

		const float rotationsPerSecond = 8.0f;
		float angleStep = angle * rotationsPerSecond * timeDelta;

		movc->m_turnRightInputAxis = (inputState->m_moveForwardAxis != 0.0f || inputState->m_moveRightAxis != 0.0f) ? angleStep : 0.0f;
		assert(!glm::isnan(movc->m_turnRightInputAxis));
	}

	// XZ movement of character
	{
		const float walkSpeed = 2.0f;
		const float sprintSpeed = 6.0f;
		const float movementSpeed = (inputState->m_sprintAction.m_down ? sprintSpeed : walkSpeed);

		glm::vec3 characterMovementDir = camera.getRightDirection() * inputState->m_moveRightAxis + camera.getForwardDirection() * inputState->m_moveForwardAxis;
		characterMovementDir.y = 0.0f;
		characterMovementDir = glm::normalize(characterMovementDir);
		characterMovementDir = glm::any(glm::isnan(characterMovementDir)) ? glm::vec3(0.0f) : characterMovementDir;
		characterMovementDir *= movementSpeed * glm::length(glm::vec2(inputState->m_moveRightAxis, inputState->m_moveForwardAxis));

		movc->m_movementXInputAxis = characterMovementDir.x;
		movc->m_movementZInputAxis = characterMovementDir.z;
	}

	// jumping
	{
		movc->m_jumpInputAction = inputState->m_jumpAction.m_pressed && movc->m_active;
	}

	// crouching
	{
		movc->m_exitCrouchInputAction = inputState->m_crouchAction.m_released;
		movc->m_enterCrouchInputAction = inputState->m_crouchAction.m_pressed;
	}

	toCamera = sphericalToCartesian(m_cameraPitch, m_cameraYaw, 1.0f);

	const float rayCastOffset = 0.31f;
	glm::vec3 rayCastOrigin = center + toCamera * rayCastOffset;
	float constrainedDistance = FLT_MAX;

	// trace a ray to see how far the camera can go away
	RayCastResult result;
	if (physics->raycast(&rayCastOrigin.x, &toCamera.x, m_cameraTargetDistance - rayCastOffset, &result))
	{
		constrainedDistance = fmaxf(result.m_rayT + rayCastOffset - 1.0f, rayCastOffset);
	}


	float actualDistance = fminf(constrainedDistance, m_cameraTargetDistance);
	actualDistance = glm::mix(fminf(constrainedDistance, m_prevCameraDistance), actualDistance, timeDelta / (timeDelta + 0.05f));
	m_prevCameraDistance = actualDistance;

	// recompute camera position
	camera.setPosition(center + toCamera * actualDistance);
	camera.lookAt(center);
}
