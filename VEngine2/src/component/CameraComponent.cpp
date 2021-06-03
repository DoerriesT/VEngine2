#include "CameraComponent.h"
#include "reflection/Reflection.h"

void CameraComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<CameraComponent>("Camera Component")
		.addField("m_fovy", &CameraComponent::m_fovy, "Field of View", "The vertical field of view.")
		.addField("m_near", &CameraComponent::m_near, "Near Plane", "The distance of the camera near plane.")
		.addField("m_far", &CameraComponent::m_far, "Far Plane", "The distance of the camera far plane.");
}