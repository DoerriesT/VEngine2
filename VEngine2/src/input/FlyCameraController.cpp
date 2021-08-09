#include "FlyCameraController.h"
#include "graphics/Camera.h"
#include "component/InputStateComponent.h"
#include "ecs/ECS.h"
#include <glm/gtc/type_ptr.hpp>

FlyCameraController::FlyCameraController(ECS *ecs)
	:m_ecs(ecs)
{
}

void FlyCameraController::update(float timeDelta, Camera &camera)
{
	const auto *inputState = m_ecs->getSingletonComponent<InputStateComponent>();

	// smooth rotation
	glm::vec2 rotateHistory = glm::mix(glm::make_vec2(m_rotateHistory), glm::vec2(inputState->m_turnRightAxis, inputState->m_lookUpAxis), timeDelta / (timeDelta + 0.05f));
	m_rotateHistory[0] = rotateHistory.x;
	m_rotateHistory[1] = rotateHistory.y;

	if ((fabsf(m_rotateHistory[0]) + fabsf(m_rotateHistory[1])) > 0.0f)
	{
		camera.rotate(glm::vec3(-m_rotateHistory[1], m_rotateHistory[0], 0.0f));
	}

	// forward is along the negative z-axis
	glm::vec3 cameraTranslation = glm::vec3(inputState->m_moveRightAxis, 0.0f, -inputState->m_moveForwardAxis);
	cameraTranslation.y += inputState->m_jumpAction.m_down ? 1.0f : 0.0f;
	cameraTranslation.y += inputState->m_crouchAction.m_down ? -1.0f : 0.0f;

	const float walkSpeed = 2.0f;
	const float sprintSpeed = 6.0f;

	cameraTranslation *= timeDelta * (inputState->m_sprintAction.m_down ? sprintSpeed : walkSpeed);

	if ((fabsf(cameraTranslation.x) + fabsf(cameraTranslation.y) + fabsf(cameraTranslation.z)) > 0.0f)
	{
		camera.translate(cameraTranslation);
	}
}
