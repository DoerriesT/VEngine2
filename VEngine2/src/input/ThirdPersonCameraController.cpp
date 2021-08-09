#include "ThirdPersonCameraController.h"
#include "ecs/ECS.h"
#include "component/InputStateComponent.h"
#include "component/CharacterMovementComponent.h"
#include "component/TransformComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include "graphics/Camera.h"
#include "Log.h"

ThirdPersonCameraController::ThirdPersonCameraController(ECS *ecs) noexcept
	:m_ecs(ecs)
{
}

void ThirdPersonCameraController::update(float timeDelta, Camera &camera, TransformComponent *characterTc, CharacterMovementComponent *movc)
{
	const auto *inputState = m_ecs->getSingletonComponent<InputStateComponent>();

	auto sphericalToCartesian = [](float pitch, float yaw, float dist)
	{
		glm::vec3 result;
		result.x = dist * cosf(yaw) * sinf(pitch);
		result.y = dist * cosf(pitch);
		result.z = dist * sinf(yaw) * sinf(pitch);
		return result;
	};

	glm::vec3 center = characterTc->m_translation + glm::vec3(0.0f, 2.0f, 0.0f);
	glm::vec3 cameraPos = sphericalToCartesian(m_cameraPitch, m_cameraYaw, m_cameraDistance);
	glm::vec3 toCamera = glm::normalize(cameraPos - center);

	m_zoomHistory = glm::mix(m_zoomHistory, inputState->m_zoomAxis, timeDelta / (timeDelta + 0.05f));
	m_cameraDistance += m_zoomHistory;
	m_cameraDistance = glm::clamp(m_cameraDistance, 1.0f, 20.0f);

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

			m_cameraPitch = glm::clamp(m_cameraPitch, glm::pi<float>() * 0.1f, glm::pi<float>() * 0.5f);
		}
	}

	// rotation of character
	{
		glm::vec3 characterForward3D = glm::mat3_cast(characterTc->m_rotation) * glm::vec3(0.0f, 1.0f, 0.0f);
		float characterYaw = atan2f(characterForward3D.z, characterForward3D.x);
		glm::vec2 toCenter = glm::normalize(glm::vec2(-toCamera.x, -toCamera.z));
		float cameraDirYaw = m_cameraYaw + glm::pi<float>();//  atan2f(toCenter.y, toCenter.x);

		movc->m_turnRightInputAxis = (inputState->m_moveForwardAxis != 0.0f || inputState->m_moveRightAxis != 0.0f) ? (cameraDirYaw - characterYaw) : 0.0f;// *timeDelta;
	}

	// XZ movement of character
	{
		const float walkSpeed = 2.0f;
		const float sprintSpeed = 6.0f;
		const float movementSpeed = timeDelta * (inputState->m_sprintAction.m_down ? sprintSpeed : walkSpeed);

		movc->m_movementForwardInputAxis = inputState->m_moveForwardAxis * movementSpeed;
		movc->m_movementRightInputAxis = inputState->m_moveRightAxis * movementSpeed;
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

	// recompute camera position
	camera.setPosition(center + sphericalToCartesian(m_cameraPitch, m_cameraYaw, m_cameraDistance));
	camera.lookAt(center);
}
