#include "CharacterControllerComponent.h"
#include <glm/trigonometric.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include <PxPhysicsAPI.h>

CharacterControllerComponent::CharacterControllerComponent(const CharacterControllerComponent &other) noexcept
	:m_slopeLimit(other.m_slopeLimit),
	m_contactOffset(other.m_contactOffset),
	m_stepOffset(other.m_stepOffset),
	m_density(other.m_density),
	m_capsuleRadius(other.m_capsuleRadius),
	m_capsuleHeight(other.m_capsuleHeight),
	m_translationHeightOffset(other.m_translationHeightOffset),
	m_active(other.m_active)
{
}

CharacterControllerComponent::CharacterControllerComponent(CharacterControllerComponent &&other) noexcept
	:m_slopeLimit(other.m_slopeLimit),
	m_contactOffset(other.m_contactOffset),
	m_stepOffset(other.m_stepOffset),
	m_density(other.m_density),
	m_capsuleRadius(other.m_capsuleRadius),
	m_capsuleHeight(other.m_capsuleHeight),
	m_translationHeightOffset(other.m_translationHeightOffset),
	m_active(other.m_active),
	m_movementDeltaX(other.m_movementDeltaX),
	m_movementDeltaY(other.m_movementDeltaY),
	m_movementDeltaZ(other.m_movementDeltaZ),
	m_collisionFlags(other.m_collisionFlags),
	m_internalControllerHandle(other.m_internalControllerHandle)
{
	other.m_internalControllerHandle = nullptr;
}

CharacterControllerComponent &CharacterControllerComponent::operator=(const CharacterControllerComponent &other) noexcept
{
	if (this != &other)
	{
		m_slopeLimit = other.m_slopeLimit;
		m_contactOffset = other.m_contactOffset;
		m_stepOffset = other.m_stepOffset;
		m_density = other.m_density;
		m_capsuleRadius = other.m_capsuleRadius;
		m_capsuleHeight = other.m_capsuleHeight;
		m_translationHeightOffset = other.m_translationHeightOffset;
		m_active = other.m_active;

		m_movementDeltaX = 0.0f;
		m_movementDeltaY = 0.0f;
		m_movementDeltaZ = 0.0f;
		m_collisionFlags = CharacterControllerCollisionFlags::NONE;

		if (m_internalControllerHandle)
		{
			((physx::PxController *)m_internalControllerHandle)->release();
			m_internalControllerHandle = nullptr;
		}
	}
	return *this;
}

CharacterControllerComponent &CharacterControllerComponent::operator=(CharacterControllerComponent &&other) noexcept
{
	if (this != &other)
	{
		m_slopeLimit = other.m_slopeLimit;
		m_contactOffset = other.m_contactOffset;
		m_stepOffset = other.m_stepOffset;
		m_density = other.m_density;
		m_capsuleRadius = other.m_capsuleRadius;
		m_capsuleHeight = other.m_capsuleHeight;
		m_translationHeightOffset = other.m_translationHeightOffset;
		m_active = other.m_active;

		m_movementDeltaX = other.m_movementDeltaX;
		m_movementDeltaY = other.m_movementDeltaY;
		m_movementDeltaZ = other.m_movementDeltaZ;
		m_collisionFlags = other.m_collisionFlags;

		if (m_internalControllerHandle)
		{
			((physx::PxController *)m_internalControllerHandle)->release();
			m_internalControllerHandle = nullptr;
		}

		m_internalControllerHandle = other.m_internalControllerHandle;
		other.m_internalControllerHandle = nullptr;
	}
	return *this;
}

CharacterControllerComponent::~CharacterControllerComponent() noexcept
{
	if (m_internalControllerHandle)
	{
		((physx::PxController *)m_internalControllerHandle)->release();
		m_internalControllerHandle = nullptr;
	}
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

	ImGui::Checkbox("Active", &c.m_active);
	ImGuiHelpers::Tooltip("If the controller is not active, the controllers position in the physics world is driven by the Transform Component. Otherwise the controller will drive the Transform Component.");

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

void CharacterControllerComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void CharacterControllerComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
