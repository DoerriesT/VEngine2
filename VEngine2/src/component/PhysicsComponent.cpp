#include "PhysicsComponent.h"
#include "reflection/Reflection.h"

void PhysicsComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<PhysicsComponent>("Physics Component");
}
