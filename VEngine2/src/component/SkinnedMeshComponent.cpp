#include "SkinnedMeshComponent.h"
#include "reflection/Reflection.h"

void SkinnedMeshComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<SkinnedMeshComponent>("Skinned Mesh Component")
		.addField("m_time", &SkinnedMeshComponent::m_time, "Time")
		.addAttribute(Attribute(AttributeType::UI_ELEMENT, AttributeUIElements::SLIDER))
		.addAttribute(Attribute(AttributeType::MIN, 0.0f))
		.addAttribute(Attribute(AttributeType::MAX, 10.0f))
		.addField("m_playing", &SkinnedMeshComponent::m_playing, "Playing")
		.addField("m_loop", &SkinnedMeshComponent::m_loop, "Loop");
}
