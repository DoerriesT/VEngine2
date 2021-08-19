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

	static void reflect(Reflection &refl) noexcept;
	static void onGUI(void *instance) noexcept;
	static const char *getComponentName() noexcept { return "LightComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Light Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};
DEFINE_TYPE_INFO(LightComponent, "04955E50-AFC4-4595-A1A2-4E4A2B16797E")