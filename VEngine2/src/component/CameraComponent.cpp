#include "CameraComponent.h"
#include "reflection/Reflection.h"
#include <glm/trigonometric.hpp>

static void fovyGetter(const void *instance, const TypeID &resultTypeID, void *result)
{
	assert(resultTypeID == getTypeID<float>());
	const CameraComponent *cc = (const CameraComponent *)instance;
	*(float *)result = glm::degrees(cc->m_fovy);
}

static void fovySetter(void *instance, const TypeID &valueTypeID, void *value)
{
	assert(valueTypeID == getTypeID<float>());
	CameraComponent *cc = (CameraComponent *)instance;
	cc->m_fovy = glm::radians(*(float *)value);
}

void CameraComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<CameraComponent>("Camera Component")
		.addField("m_fovy", &CameraComponent::m_fovy, "Field of View", "The vertical field of view.")
		.addAttribute(Attribute(AttributeType::MIN, 1.0f))
		.addAttribute(Attribute(AttributeType::MAX, 179.0f))
		.addAttribute(Attribute(AttributeType::GETTER, fovyGetter))
		.addAttribute(Attribute(AttributeType::SETTER, fovySetter))
		.addField("m_near", &CameraComponent::m_near, "Near Plane", "The distance of the camera near plane.")
		.addAttribute(Attribute(AttributeType::MIN, 0.1f))
		.addField("m_far", &CameraComponent::m_far, "Far Plane", "The distance of the camera far plane.")
		.addAttribute(Attribute(AttributeType::MIN, 0.2f));
}