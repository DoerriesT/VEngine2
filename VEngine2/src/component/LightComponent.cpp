#include "LightComponent.h"
#include <glm/trigonometric.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include <EASTL/algorithm.h>
#include "TransformComponent.h"
#include "graphics/Renderer.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(LightComponent &c, Stream &stream) noexcept
{
	uint32_t typeInt = static_cast<uint32_t>(c.m_type);
	serializeUInt32(stream, typeInt);
	c.m_type = static_cast<LightComponent::Type>(typeInt);
	serializeUInt32(stream, c.m_color);
	serializeFloat(stream, c.m_intensity);
	serializeBool(stream, c.m_shadows);
	serializeFloat(stream, c.m_radius);
	serializeFloat(stream, c.m_outerAngle);
	serializeFloat(stream, c.m_innerAngle);
	serializeFloat(stream, c.m_splitLambda);
	serializeUInt32(stream, c.m_cascadeCount);
	c.m_cascadeCount = eastl::min<uint32_t>(c.m_cascadeCount, LightComponent::k_maxCascades);
	for (size_t i = 0; i < c.m_cascadeCount; ++i)
	{
		serializeFloat(stream, c.m_depthBias[i]);
		serializeFloat(stream, c.m_normalOffsetBias[i]);
	}
	serializeFloat(stream, c.m_maxShadowDistance);

	return true;
}

void LightComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	LightComponent &c = *reinterpret_cast<LightComponent *>(instance);

	int typeInt = static_cast<int>(c.m_type);
	ImGui::RadioButton("Point", &typeInt, static_cast<int>(LightComponent::Type::Point)); ImGui::SameLine();
	ImGui::RadioButton("Spot", &typeInt, static_cast<int>(LightComponent::Type::Spot)); ImGui::SameLine();
	ImGui::RadioButton("Directional", &typeInt, static_cast<int>(LightComponent::Type::Directional));
	c.m_type = static_cast<LightComponent::Type>(typeInt);

	ImGuiHelpers::ColorEdit3("Color", &c.m_color);
	ImGuiHelpers::Tooltip("The sRGB color of the light source.");

	ImGui::DragFloat("Intensity", &c.m_intensity, 1.0f, 0.0f, FLT_MAX);
	ImGuiHelpers::Tooltip("Intensity as luminous power (in lumens).");

	ImGui::Checkbox("Casts Shadows", &c.m_shadows);
	ImGuiHelpers::Tooltip("Enables shadows for this light source.");

	if (c.m_type == Type::Directional)
	{
		ImGui::DragFloat("Split Lambda", &c.m_splitLambda, 0.05f, 0.0f, 1.0f);

		int cascades = static_cast<int>(c.m_cascadeCount);
		if (ImGui::InputInt("Cascades", &cascades, 1, 1))
		{
			c.m_cascadeCount = eastl::clamp<int>(cascades, 1, LightComponent::k_maxCascades);
		}

		for (size_t i = 0; i < c.m_cascadeCount; ++i)
		{
			char cascadeLabel[] = "Cascade X";
			cascadeLabel[sizeof(cascadeLabel) - 2] = '0' + static_cast<char>(i);
			ImGui::Text(cascadeLabel);
			ImGui::PushID(&c.m_depthBias[i]);
			ImGui::DragFloat("Depth Bias", &c.m_depthBias[i], 0.05f, 0.0f, 20.0f);
			ImGui::PopID();
			ImGui::PushID(&c.m_normalOffsetBias[i]);
			ImGui::DragFloat("Normal Offset Bias", &c.m_normalOffsetBias[i], 0.05f, 0.0f, 20.0f);
			ImGui::PopID();
		}

		ImGui::DragFloat("Shadow Distance", &c.m_maxShadowDistance, 0.05f, 0.0f, FLT_MAX);
	}
	else
	{
		ImGui::DragFloat("Radius", &c.m_radius, 1.0f, 0.01f, FLT_MAX);
		ImGuiHelpers::Tooltip("The radius of the sphere of influence of the light source.");

		if (c.m_type == Type::Spot)
		{
			float outerAngleDegrees = glm::degrees(c.m_outerAngle);
			ImGui::DragFloat("Outer Spot Angle", &outerAngleDegrees, 1.0f, 1.0f, 179.0f);
			c.m_outerAngle = glm::radians(outerAngleDegrees);
			ImGuiHelpers::Tooltip("The outer angle of the spot light cone.");

			float innerAngleDegrees = glm::degrees(c.m_innerAngle);
			ImGui::DragFloat("Inner Spot Angle", &innerAngleDegrees, 1.0f, 1.0f, outerAngleDegrees);
			c.m_innerAngle = glm::radians(innerAngleDegrees);
			ImGuiHelpers::Tooltip("The inner angle of the spot light cone.");
		}
	}

	if (renderer && transformComponent)
	{
		const glm::vec4 k_visibleLightDebugColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
		const glm::vec4 k_occludedLightDebugColor = glm::vec4(0.5f, 0.25f, 0.0f, 1.0f);

		const auto &transform = transformComponent->m_globalTransform;

		switch (c.m_type)
		{
		case Type::Point: renderer->drawDebugSphere(transform.m_translation, transform.m_rotation, glm::vec3(c.m_radius), k_visibleLightDebugColor, k_occludedLightDebugColor, true); break;
		case Type::Spot: renderer->drawDebugCappedCone(transform.m_translation, transform.m_rotation, c.m_radius, c.m_outerAngle, k_visibleLightDebugColor, k_occludedLightDebugColor, true); break;
		case Type::Directional: renderer->drawDebugArrow(transform.m_translation, transform.m_rotation, 3.0f, k_visibleLightDebugColor, k_occludedLightDebugColor, true); break;
		default:
			assert(false);
			break;
		}
	}
}

bool LightComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<LightComponent *>(instance), stream);
}

bool LightComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<LightComponent *>(instance), stream);
}

void LightComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}

void LightComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}
