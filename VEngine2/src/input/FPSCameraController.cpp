#include "FPSCameraController.h"
#include "graphics/Camera.h"
#include "UserInput.h"
#include <glm/gtc/type_ptr.hpp>
#include "component/CharacterControllerComponent.h"
#include "component/PlayerMovementComponent.h"
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
		cameraTranslation.z = -mod;
		pressed = true;
	}
	if (inputState->isKeyDown(InputKey::S))
	{
		cameraTranslation.z = mod;
		pressed = true;
	}
	if (inputState->isKeyDown(InputKey::A))
	{
		cameraTranslation.x = -mod;
		pressed = true;
	}
	if (inputState->isKeyDown(InputKey::D))
	{
		cameraTranslation.x = mod;
		pressed = true;
	}

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
		moveDirection *= mod * 5.0f;

		m_ecs->iterate<CharacterControllerComponent, PlayerMovementComponent>([&](size_t count, const EntityID *entities, CharacterControllerComponent *ccC, PlayerMovementComponent *movC)
			{
				for (size_t i = 0; i < count; ++i)
				{
					auto &mc = movC[i];
					auto &cc = ccC[i];

					const bool touchesGround = (cc.m_collisionFlags & CharacterControllerCollisionFlags::DOWN) != 0;
					const bool touchesCeiling = (cc.m_collisionFlags & CharacterControllerCollisionFlags::UP) != 0;

					// any downwards velocity is cancelled by touching the ground
					if (touchesGround)
					{
						mc.m_velocityY = std::max(mc.m_velocityY, 0.0f);
					}

					if (mc.m_active)
					{
						if (mc.m_jumping)
						{
							// we just hit our head
							if (mc.m_jumping && touchesCeiling)
							{
  								mc.m_velocityY = std::min(mc.m_velocityY, 0.0f);
							}

							// our jump is over
							if (mc.m_jumping && touchesGround)
							{
								mc.m_jumping = false;
							}
						}
						if (inputState->isKeyPressed(InputKey::SPACE))
						{
							mc.m_velocityY += 4.0f;
							mc.m_jumping = true;
						}

						const float controllerHeight = 1.8f;
						const float cameraHeight = controllerHeight - 0.1f;
						const float crouchedControllerHeight = controllerHeight * 0.5f;
						const float crouchedCameraHeight = crouchedControllerHeight - 0.1f;

						const bool crouchKeyDown = inputState->isKeyDown(InputKey::LEFT_CONTROL);

						switch (mc.m_crouchState)
						{
						case CrouchState::STANDING:
							if (crouchKeyDown)
							{
								mc.m_crouchState = CrouchState::ENTERING_CROUCH;
							}
							break;
						case CrouchState::ENTERING_CROUCH:
							mc.m_crouchPercentage += timeDelta * 4.0f;
							mc.m_crouchPercentage = std::min(mc.m_crouchPercentage, 1.0f);
							if (mc.m_crouchPercentage == 1.0f)
							{
								mc.m_crouchState = CrouchState::CROUCHING;
							}
							break;
						case CrouchState::CROUCHING:
							if (!crouchKeyDown)
							{
								mc.m_crouchState = CrouchState::EXITING_CROUCH;
							}
							break;
						case CrouchState::EXITING_CROUCH:
							mc.m_crouchPercentage -= timeDelta * 4.0f;
							mc.m_crouchPercentage = std::max(mc.m_crouchPercentage, 0.0f);
							if (mc.m_crouchPercentage == 0.0f)
							{
								mc.m_crouchState = CrouchState::STANDING;
							}
							break;
						default:
							assert(false);
							break;
						}

						cc.m_capsuleHeight = controllerHeight * (1.0f - mc.m_crouchPercentage) + crouchedControllerHeight * mc.m_crouchPercentage;
						cc.m_cameraHeight = cameraHeight * (1.0f - mc.m_crouchPercentage) + crouchedCameraHeight * mc.m_crouchPercentage;

						cc.m_movementDeltaX = moveDirection.x * timeDelta;
						cc.m_movementDeltaZ = moveDirection.z * timeDelta;
					}

					// gravity alway applies to our velocity
					mc.m_velocityY += -9.81f * timeDelta;

					cc.m_movementDeltaY = mc.m_velocityY * timeDelta;
				}
			});
	}
	else if (pressed)
	{
		camera.translate(cameraTranslation);
	}
}
