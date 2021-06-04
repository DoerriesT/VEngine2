#include "TransformComponent.h"
#include "reflection/Reflection.h"

void TransformComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<TransformComponent>("Transform Component")
		.addField("m_translation", &TransformComponent::m_translation, "Translation", "The translation of the entity.")
		.addField("m_rotation", &TransformComponent::m_rotation, "Rotation", "The rotation in euler angles of the entity.")
		.addAttribute(Attribute(AttributeType::UI_ELEMENT, AttributeUIElements::EULER_ANGLES))
		.addField("m_scale", &TransformComponent::m_scale, "Scale", "The scale of the entity.");
}