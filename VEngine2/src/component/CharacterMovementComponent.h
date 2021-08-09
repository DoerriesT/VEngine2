#pragma once
#include "reflection/TypeInfo.h"

class Reflection;

enum class CrouchState : uint32_t
{
	STANDING,
	ENTERING_CROUCH,
	CROUCHING,
	EXITING_CROUCH
};

struct CharacterMovementComponent
{
	// internal
	CrouchState m_crouchState;
	float m_crouchPercentage;
	float m_velocityY;
	bool m_active;
	bool m_jumping;

	// input
	float m_movementForwardInputAxis;
	float m_movementRightInputAxis;
	float m_turnRightInputAxis;
	bool m_jumpInputAction;
	bool m_enterCrouchInputAction;
	bool m_exitCrouchInputAction;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(CharacterMovementComponent, "EDFA641B-06BD-4D34-A15B-B749DE8668D5")