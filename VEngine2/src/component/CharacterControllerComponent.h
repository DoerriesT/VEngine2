#pragma once
#include "reflection/TypeInfo.h"
#include "utility/Enum.h"

class Reflection;

enum class CharacterControllerCollisionFlags
{
	NONE = 0,
	SIDES = (1 << 0),
	UP = (1 << 1),
	DOWN = (1 << 2)
};
DEF_ENUM_FLAG_OPERATORS(CharacterControllerCollisionFlags);


struct CharacterControllerComponent
{
	// creation parameters
	float m_slopeLimit = 0.785398f; // 45 degrees in radians
	float m_contactOffset = 0.1f;
	float m_stepOffset = 0.5f;
	float m_density = 10.0f;
	float m_capsuleRadius = 0.2f;
	float m_capsuleHeight = 1.8f;
	float m_translationHeightOffset = 1.7f; // offset to apply to the y-component of the resulting "feet" position of the capsule when setting the translation

	// per frame input
	float m_movementDeltaX = 0.0f;
	float m_movementDeltaY = 0.0f;
	float m_movementDeltaZ = 0.0f;

	// per frame output
	CharacterControllerCollisionFlags m_collisionFlags = CharacterControllerCollisionFlags::NONE;

	// internal
	void *m_internalControllerHandle = nullptr;

	static void reflect(Reflection &refl) noexcept;

	static void onGUI(void *instance) noexcept;
	static const char *getComponentName() noexcept { return "CharacterControllerComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Character Controller Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};
DEFINE_TYPE_INFO(CharacterControllerComponent, "3905864D-D838-418A-BC65-BFDD8FCE37CB")