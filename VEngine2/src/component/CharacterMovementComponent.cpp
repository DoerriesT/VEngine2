#include "CharacterMovementComponent.h"
#include "reflection/Reflection.h"

void CharacterMovementComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<CharacterMovementComponent>("Character Movement Component")
		.addField("m_velocityX", &CharacterMovementComponent::m_velocityX, "Velocity X")
		.addField("m_velocityY", &CharacterMovementComponent::m_velocityY, "Velocity Y")
		.addField("m_velocityZ", &CharacterMovementComponent::m_velocityZ, "Velocity Z")
		.addField("m_active", &CharacterMovementComponent::m_active, "Active")
		.addField("m_jumping", &CharacterMovementComponent::m_jumping, "Jumping");
}
