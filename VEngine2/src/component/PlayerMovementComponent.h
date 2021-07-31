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

struct PlayerMovementComponent
{
	CrouchState m_crouchState;
	float m_crouchPercentage;
	float m_velocityY;
	bool m_active;
	bool m_jumping;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(PlayerMovementComponent, "EDFA641B-06BD-4D34-A15B-B749DE8668D5")