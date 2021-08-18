#include "FPSCameraController.h"
#include "graphics/Camera.h"
#include <glm/gtc/type_ptr.hpp>
#include "component/CharacterControllerComponent.h"
#include "component/CharacterMovementComponent.h"
#include "component/InputStateComponent.h"
#include "ecs/ECS.h"

FPSCameraController::FPSCameraController(ECS *ecs) noexcept
	:m_ecs(ecs)
{
}

void FPSCameraController::update(float timeDelta, Camera &camera, CharacterMovementComponent *movc) noexcept
{
	const auto *inputState = m_ecs->getSingletonComponent<InputStateComponent>();

	// rotation
	{
		// smooth rotation
		glm::vec2 rotateHistory = glm::mix(glm::make_vec2(m_rotateHistory), glm::vec2(inputState->m_turnRightAxis, inputState->m_lookUpAxis), timeDelta / (timeDelta + 0.05f));
		m_rotateHistory[0] = rotateHistory.x;
		m_rotateHistory[1] = rotateHistory.y;

		// apply rotation
		if ((fabsf(m_rotateHistory[0]) + fabsf(m_rotateHistory[1])) > 0.0f)
		{
			camera.rotate(glm::vec3(-m_rotateHistory[1], m_rotateHistory[0], 0.0f));
		}
	}

	// XZ movement
	{
		const float walkSpeed = 2.0f;
		const float sprintSpeed = 6.0f;
		const float movementSpeed = (inputState->m_sprintAction.m_down ? sprintSpeed : walkSpeed);

		glm::vec3 characterMovementDir = camera.getRightDirection() * inputState->m_moveRightAxis + camera.getForwardDirection() * inputState->m_moveForwardAxis;
		characterMovementDir.y = 0.0f;
		characterMovementDir = glm::normalize(characterMovementDir);
		characterMovementDir = glm::any(glm::isnan(characterMovementDir)) ? glm::vec3(0.0f) : characterMovementDir;
		characterMovementDir *= movementSpeed;

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
}
