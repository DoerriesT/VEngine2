#pragma once
#include "reflection/TypeInfo.h"

class Reflection;

struct LightComponent
{
	uint32_t m_color = 0xFFFFFFFF;
	float m_luminousPower = 1000.0f;
	float m_radius = 8.0f;
	float m_outerAngle = 0.785f;
	float m_innerAngle = 0.26f;
	bool m_shadows = false;
	bool m_volumetricShadows = false; // requires m_shadows to be true

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(LightComponent, "04955E50-AFC4-4595-A1A2-4E4A2B16797E")