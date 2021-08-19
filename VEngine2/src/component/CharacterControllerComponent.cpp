#include "CharacterControllerComponent.h"
#include "reflection/Reflection.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void CharacterControllerComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<CharacterControllerComponent>("Character Controller Component");
}

void CharacterControllerComponent::onGUI(void *instance) noexcept
{
	CharacterControllerComponent &c = *reinterpret_cast<CharacterControllerComponent *>(instance);

	float slopeDegrees = glm::degrees(c.m_slopeLimit);
	ImGui::DragFloat("Slope Limit", &slopeDegrees, 1.0f, 0.0f, 90.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	c.m_slopeLimit = glm::radians(slopeDegrees);
	ImGuiHelpers::Tooltip("The maximum slope which the character can walk up.");

	ImGui::DragFloat("Contact Offset", &c.m_contactOffset, 0.01f, 0.0f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The contact offset used by the controller. Specifies a skin around the object within which contacts will be generated. Use it to avoid numerical precision issues.");

	ImGui::DragFloat("Step Offset", &c.m_stepOffset, 0.01f, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("Defines the maximum height of an obstacle which the character can climb.");

	ImGui::DragFloat("Density", &c.m_density, 0.1f, 0.1f, 20.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("Density of underlying kinematic actor.");

	ImGui::DragFloat("Capsule Radius", &c.m_capsuleRadius, 0.05f, 0.05f, 5.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The radius of the capsule.");

	ImGui::DragFloat("Capsule Height", &c.m_capsuleHeight, 0.05f, 0.05f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The total height of the capsule. Includes both end caps.");

	ImGui::DragFloat("Translation Height Offset", &c.m_translationHeightOffset, 0.05f, -10.0f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The offset to apply to the y-component of the resulting \"feet\" position of the capsule when setting the translation of the attached TransformComponent.");

	ImGui::Separator();

	float dummy3[] = { c.m_movementDeltaX, c.m_movementDeltaY, c.m_movementDeltaZ };
	ImGui::InputFloat3("Movement Delta", dummy3, "%.3f");

	bool collisionSides = (c.m_collisionFlags & CharacterControllerCollisionFlags::SIDES) != 0;
	ImGui::Checkbox("Collision Sideways", &collisionSides);
	bool collisionUp = (c.m_collisionFlags & CharacterControllerCollisionFlags::UP) != 0;
	ImGui::Checkbox("Collision Up", &collisionUp);
	bool collisionDown = (c.m_collisionFlags & CharacterControllerCollisionFlags::DOWN) != 0;
	ImGui::Checkbox("Collision Down", &collisionDown);
}
