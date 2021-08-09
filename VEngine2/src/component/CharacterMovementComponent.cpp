#include "CharacterMovementComponent.h"
#include "reflection/Reflection.h"

void CharacterMovementComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<CharacterMovementComponent>("Character Movement Component")
		.addField("m_velocityY", &CharacterMovementComponent::m_velocityY, "Velocity Y")
		.addField("m_active", &CharacterMovementComponent::m_active, "Active")
		.addField("m_jumping", &CharacterMovementComponent::m_jumping, "Jumping");
}
