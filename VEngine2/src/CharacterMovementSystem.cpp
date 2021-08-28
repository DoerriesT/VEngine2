#include "CharacterMovementSystem.h"
#include "ecs/ECS.h"
#include "component/TransformComponent.h"
#include "component/CharacterControllerComponent.h"
#include "component/CharacterMovementComponent.h"
#include <glm/gtc/type_ptr.hpp>
#include "Log.h"

CharacterMovementSystem::CharacterMovementSystem(ECS *ecs) noexcept
	:m_ecs(ecs)
{
}

void CharacterMovementSystem::update(float timeDelta) noexcept
{
	m_ecs->iterate<TransformComponent, CharacterMovementComponent, CharacterControllerComponent>(
		[timeDelta](size_t count, const EntityID *entities, TransformComponent *transformComps, CharacterMovementComponent *movementComps, CharacterControllerComponent *controllerComps)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transformComps[i];
				auto &mc = movementComps[i];
				auto &cc = controllerComps[i];

				// rotation
				{
					glm::quat turnQuat = glm::quat(glm::angleAxis(-mc.m_turnRightInputAxis, glm::vec3(0.0f, 1.0f, 0.0f)));
					tc.m_rotation = glm::normalize(turnQuat * tc.m_rotation);
				}

				mc.m_velocityX = 0.0f;
				mc.m_velocityZ = 0.0f;

				// XZ movement
				{
					// preliminary movement delta
					mc.m_velocityX = mc.m_movementXInputAxis;
					mc.m_velocityZ = mc.m_movementZInputAxis;
				}


				const bool touchesGround = (cc.m_collisionFlags & CharacterControllerCollisionFlags::DOWN) != 0;
				const bool touchesCeiling = (cc.m_collisionFlags & CharacterControllerCollisionFlags::UP) != 0;

				float oldVelocityY = mc.m_velocityY;
				const float gravity = -9.81f;

				// any downwards velocity is cancelled by touching the ground
				if (touchesGround)
				{
					mc.m_velocityY = std::max(mc.m_velocityY, 0.0f);
				}
				
				// any upwards velocity is cancelled by hitting our head
				if (touchesCeiling)
				{
					mc.m_velocityY = std::min(mc.m_velocityY, 0.0f);
				}

				// apply gravity
				{
					mc.m_velocityY += gravity * timeDelta;
				}

				// jumping
				{
					if (mc.m_jumping)
					{
						// our jump is over
						if (mc.m_jumping && touchesGround)
						{
							mc.m_jumping = false;
						}
					}
					/*else*/ if (/*!mc.m_jumping &&*/ mc.m_jumpInputAction)
					{
						const float jumpHeight = 2.0f;
						const float jumpVelocity0 = sqrtf(2.0f * fabsf(gravity) * jumpHeight);

						mc.m_velocityY = jumpVelocity0;
						mc.m_jumping = true;
					}
				}

				// crouching state machine
				{
					const float controllerHeight = 1.8f;
					const float cameraHeight = controllerHeight - 0.1f;
					const float crouchedControllerHeight = controllerHeight * 0.5f;
					const float crouchedCameraHeight = crouchedControllerHeight - 0.1f;
					const float crouchEnterExitSpeed = 4.0f;

					switch (mc.m_crouchState)
					{
					case CrouchState::STANDING:
						cc.m_capsuleHeight = controllerHeight;
						if (mc.m_enterCrouchInputAction)
						{
							mc.m_crouchState = CrouchState::ENTERING_CROUCH;
						}
						break;
					case CrouchState::ENTERING_CROUCH:
						mc.m_crouchPercentage += timeDelta * crouchEnterExitSpeed;
						mc.m_crouchPercentage = std::min(mc.m_crouchPercentage, 1.0f);
						if (mc.m_crouchPercentage == 1.0f)
						{
							mc.m_crouchState = CrouchState::CROUCHING;
						}
						break;
					case CrouchState::CROUCHING:
						cc.m_capsuleHeight = crouchedControllerHeight;
						if (mc.m_exitCrouchInputAction)
						{
							mc.m_crouchState = CrouchState::EXITING_CROUCH;
						}
						break;
					case CrouchState::EXITING_CROUCH:
						mc.m_crouchPercentage -= timeDelta * crouchEnterExitSpeed;
						mc.m_crouchPercentage = std::max(mc.m_crouchPercentage, 0.0f);
						if (mc.m_crouchPercentage == 0.0f)
						{
							mc.m_crouchState = CrouchState::STANDING;
							cc.m_capsuleHeight = controllerHeight; // change directly to standing controller height
						}
						break;
					default:
						assert(false);
						break;
					}

					//cc.m_translationHeightOffset = glm::mix(cameraHeight, crouchedCameraHeight, mc.m_crouchPercentage);
				}

				cc.m_movementDeltaX = mc.m_velocityX * timeDelta;
				cc.m_movementDeltaY = (oldVelocityY + mc.m_velocityY) * 0.5f * timeDelta;
				cc.m_movementDeltaZ = mc.m_velocityZ * timeDelta;
			}
		});
}
