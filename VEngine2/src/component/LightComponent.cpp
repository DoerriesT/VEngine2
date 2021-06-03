#include "LightComponent.h"
#include "reflection/Reflection.h"

void LightComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<LightComponent>("Light Component", "Represents a light source.")
		.addField("m_color", &LightComponent::m_color, "Color")
		.addField("m_luminousPower", &LightComponent::m_luminousPower, "Intensity", "Intensity in lumens.")
		.addField("m_outerAngle", &LightComponent::m_outerAngle, "Outer Angle", "Outer angle of the spot light.")
		.addField("m_innerAngle", &LightComponent::m_innerAngle, "Inner Radius", "Inner angle of the spot light.")
		.addField("m_shadows", &LightComponent::m_shadows, "Shadows", "Enables shadow casting for this light.")
		.addField("m_volumetricShadows", &LightComponent::m_volumetricShadows, "Volumetric Radius", "Enables volumetric shadow casting for this light.");
}
