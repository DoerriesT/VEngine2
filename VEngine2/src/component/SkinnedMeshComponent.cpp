#include "SkinnedMeshComponent.h"
#include "reflection/Reflection.h"

void SkinnedMeshComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<SkinnedMeshComponent>("Skinned Mesh Component");
}
