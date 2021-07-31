#include "PlayerMovementComponent.h"
#include "reflection/Reflection.h"

void PlayerMovementComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<PlayerMovementComponent>("Player Movement Component")
		.addField("m_velocityY", &PlayerMovementComponent::m_velocityY, "Velocity Y")
		.addField("m_active", &PlayerMovementComponent::m_active, "Active")
		.addField("m_jumping", &PlayerMovementComponent::m_jumping, "Jumping");
}
