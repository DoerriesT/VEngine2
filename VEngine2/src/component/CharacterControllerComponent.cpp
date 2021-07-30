#include "CharacterControllerComponent.h"
#include "reflection/Reflection.h"

void CharacterControllerComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<CharacterControllerComponent>("Character Controller Component");
}
