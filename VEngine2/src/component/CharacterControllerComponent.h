#pragma once
#include "reflection/TypeInfo.h"

class Reflection;

struct CharacterControllerComponent
{
	float m_slopeLimit = 0.707f; // as cos(slope)
	float m_contactOffset = 0.1f;
	float m_stepOffset = 0.5f;
	float m_density = 10.0f;
	float m_movementDeltaX = 0.0f;
	float m_movementDeltaY = 0.0f;
	float m_movementDeltaZ = 0.0f;
	bool m_touchesGround = false;
	bool m_jumping = false;
	float m_jumpTime = 0.0f;
	float m_jumpV0;
	void *m_internalControllerHandle = nullptr;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(CharacterControllerComponent, "3905864D-D838-418A-BC65-BFDD8FCE37CB")