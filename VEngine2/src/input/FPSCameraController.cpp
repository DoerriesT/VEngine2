#include "FPSCameraController.h"
#include "graphics/Camera.h"
#include "UserInput.h"
#include <glm/gtc/type_ptr.hpp>
#include "component/CharacterControllerComponent.h"
#include "component/InputStateComponent.h"
#include "ecs/ECS.h"

FPSCameraController::FPSCameraController(ECS *ecs)
	:m_ecs(ecs)
{
}

void FPSCameraController::update(float timeDelta, Camera &camera, CharacterControllerComponent *ccc)
{
	const auto *inputState = m_ecs->getSingletonComponent<RawInputStateComponent>();

	bool pressed = false;
	float mod = 1.0f;
	glm::vec3 cameraTranslation = glm::vec3(0.0f);

	glm::vec2 mouseDelta = {};

	if (inputState->isMouseButtonDown(InputMouse::BUTTON_RIGHT))
	{
		if (!m_grabbedMouse)
		{
			m_grabbedMouse = true;
			//m_window->setMouseCursorMode(Window::MouseCursorMode::DISABLED);
		}
		mouseDelta = glm::vec2(inputState->m_mousePosDeltaX, inputState->m_mousePosDeltaY);
	}
	else
	{
		if (m_grabbedMouse)
		{
			m_grabbedMouse = false;
			//m_window->setMouseCursorMode(Window::MouseCursorMode::NORMAL);
		}
	}

	glm::vec2 mouseHistory = glm::mix(glm::make_vec2(m_mouseHistory), mouseDelta, timeDelta / (timeDelta + 0.05f));
	if (glm::dot(mouseHistory, mouseHistory) > 0.0f)
	{
		camera.rotate(glm::vec3(mouseHistory.y * 0.005f, mouseHistory.x * 0.005f, 0.0));
	}
	m_mouseHistory[0] = mouseHistory[0];
	m_mouseHistory[1] = mouseHistory[1];


	if (inputState->isKeyDown(InputKey::UP))
	{
		camera.rotate(glm::vec3(-timeDelta, 0.0f, 0.0));
	}
	if (inputState->isKeyDown(InputKey::DOWN))
	{
		camera.rotate(glm::vec3(timeDelta, 0.0f, 0.0));
	}
	if (inputState->isKeyDown(InputKey::LEFT))
	{
		camera.rotate(glm::vec3(0.0f, -timeDelta, 0.0));
	}
	if (inputState->isKeyDown(InputKey::RIGHT))
	{
		camera.rotate(glm::vec3(0.0f, timeDelta, 0.0));
	}

	if (inputState->isKeyDown(InputKey::LEFT_SHIFT))
	{
		mod = 5.0f;
	}
	if (inputState->isKeyDown(InputKey::W))
	{
		cameraTranslation.z = -mod * (float)timeDelta;
		pressed = true;
	}
	if (inputState->isKeyDown(InputKey::S))
	{
		cameraTranslation.z = mod * (float)timeDelta;
		pressed = true;
	}
	if (inputState->isKeyDown(InputKey::A))
	{
		cameraTranslation.x = -mod * (float)timeDelta;
		pressed = true;
	}
	if (inputState->isKeyDown(InputKey::D))
	{
		cameraTranslation.x = mod * (float)timeDelta;
		pressed = true;
	}

	if (ccc)
	{
		ccc->m_movementDeltaX = 0.0f;
		ccc->m_movementDeltaY = 0.0f;
		ccc->m_movementDeltaZ = 0.0f;
	}

	//if (pressed)
	{
		if (ccc)
		{
			auto viewMat = camera.getViewMatrix();

			glm::vec3 forward(viewMat[0][2], viewMat[1][2], viewMat[2][2]);
			glm::vec3 strafe(viewMat[0][0], viewMat[1][0], viewMat[2][0]);

			glm::vec3 moveDirection = cameraTranslation.z * forward + cameraTranslation.x * strafe;
			glm::vec2 moveDir2D = glm::normalize(glm::vec2(moveDirection.x, moveDirection.z));
			moveDir2D = glm::any(glm::isnan(moveDir2D)) ? glm::vec2(0.0f) : moveDir2D;
			moveDirection.x = moveDir2D.x;
			moveDirection.y = 0.0f;
			moveDirection.z = moveDir2D.y;
			moveDirection *= mod;

			//printf("%d\n", (int)ccc->m_touchesGround);

			if (ccc->m_jumping && ccc->m_touchesGround)
			{
				ccc->m_jumping = false;
			}

			if (ccc->m_touchesGround && inputState->isKeyPressed(InputKey::SPACE))
			{
				ccc->m_jumping = true;
				ccc->m_jumpTime = 0.0f;
				ccc->m_jumpV0 = 80.0f;
			}


			float jumpHeightDelta = 0.0f;
			float dy = 0.0f;

			if (ccc->m_jumping)
			{
				ccc->m_jumpTime += timeDelta;
				const float jumpGravity = -50.0f;
				jumpHeightDelta = jumpGravity * ccc->m_jumpTime * ccc->m_jumpTime + ccc->m_jumpV0 * ccc->m_jumpTime;
			}

			if (jumpHeightDelta != 0.0f)
			{
				dy = jumpHeightDelta;
			}
			else
			{
				dy = -9.81f;
			}

			moveDirection.y = dy;
			moveDirection *= (float)timeDelta;

			ccc->m_movementDeltaX = moveDirection.x;
			ccc->m_movementDeltaY = moveDirection.y;
			ccc->m_movementDeltaZ = moveDirection.z;
		}
		else
		{
			camera.translate(cameraTranslation);
		}
	}
}
