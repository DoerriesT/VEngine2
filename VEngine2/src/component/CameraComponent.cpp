#include "CameraComponent.h"
#include "reflection/Reflection.h"
#include <glm/trigonometric.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

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

void CameraComponent::onGUI(void *instance) noexcept
{
	CameraComponent &cc = *reinterpret_cast<CameraComponent *>(instance);

	float degrees = glm::degrees(cc.m_fovy);
	ImGui::DragFloat("Field of View", &degrees, 1.0f, 1.0f, 179.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	cc.m_fovy = glm::radians(degrees);
	ImGuiHelpers::Tooltip("The vertical field of view.");

	ImGui::DragFloat("Near Plane", &cc.m_near, 0.05f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the camera near plane.");

	ImGui::DragFloat("Far Plane", &cc.m_far, 0.1f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the camera far plane.");
}
