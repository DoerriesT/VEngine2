#include "MeshComponent.h"
#include "reflection/Reflection.h"

void MeshComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<MeshComponent>("Mesh Component");
}
