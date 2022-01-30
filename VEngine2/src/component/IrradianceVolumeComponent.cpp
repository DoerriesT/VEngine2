#include "IrradianceVolumeComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"
#include "utility/Serialization.h"
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include "TransformComponent.h"
#include "graphics/Renderer.h"

template<typename Stream>
static bool serialize(IrradianceVolumeComponent &c, Stream &stream) noexcept
{
	serializeUInt32(stream, c.m_resolutionX);
	serializeUInt32(stream, c.m_resolutionY);
	serializeUInt32(stream, c.m_resolutionZ);
	serializeUInt32(stream, c.m_selfShadowBias);
	serializeFloat(stream, c.m_nearPlane);
	serializeFloat(stream, c.m_farPlane);

	return true;
}

void IrradianceVolumeComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	IrradianceVolumeComponent &c = *reinterpret_cast<IrradianceVolumeComponent *>(instance);

	int ival;
	
	ival = static_cast<int>(c.m_resolutionX);
	if (ImGui::InputInt("Resolution X", &ival, 1, 5))
	{
		c.m_resolutionX = eastl::max<int>(ival, 1);
	}
	ival = static_cast<int>(c.m_resolutionY);
	if (ImGui::InputInt("Resolution Y", &ival, 1, 5))
	{
		c.m_resolutionY = eastl::max<int>(ival, 1);
	}
	ival = static_cast<int>(c.m_resolutionZ);
	if (ImGui::InputInt("Resolution Z", &ival, 1, 5))
	{
		c.m_resolutionZ = eastl::max<int>(ival, 1);
	}

	ImGui::DragFloat("Self Shadow Bias", &c.m_selfShadowBias, 0.01f, 0.0f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("Bias to avoid self shadowing in anti light leaking algorithm.");

	ImGui::DragFloat("Near Plane", &c.m_nearPlane, 0.05f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the probes near plane when baking.");

	ImGui::DragFloat("Far Plane", &c.m_farPlane, 0.1f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the probes far plane when baking.");

	if (renderer && transformComponent)
	{
		const glm::vec4 k_visibleDebugColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
		const glm::vec4 k_occludedDebugColor = glm::vec4(0.5f, 0.25f, 0.0f, 1.0f);

		auto &transform = transformComponent->m_transform;
		glm::mat4 boxTransform = glm::translate(transform.m_translation) * glm::mat4_cast(transform.m_rotation) * glm::scale(transform.m_scale);
		renderer->drawDebugBox(boxTransform, k_visibleDebugColor, k_occludedDebugColor, true);
		renderer->drawDebugBox(boxTransform, glm::vec4(1.0f, 0.5f, 0.0f, 0.125f), glm::vec4(0.5f, 0.25f, 0.0f, 0.125f), true, false);

		glm::vec3 probeSpacing = transform.m_scale / (glm::vec3(c.m_resolutionX, c.m_resolutionY, c.m_resolutionZ) * 0.5f);
		glm::vec3 volumeOrigin = transform.m_translation - (glm::mat3_cast(transform.m_rotation) * transform.m_scale);
		glm::mat4 localToWorld = glm::translate(volumeOrigin) * glm::mat4_cast(transform.m_rotation) * glm::scale(probeSpacing);

		for (size_t z = 0; z < c.m_resolutionZ; ++z)
		{
			for (size_t y = 0; y < c.m_resolutionY; ++y)
			{
				for (size_t x = 0; x < c.m_resolutionX; ++x)
				{
					

					glm::vec3 probeWorldSpacePos = glm::vec3(localToWorld * glm::vec4(x, y, z, 1.0f));
					renderer->drawDebugCross(probeWorldSpacePos, 0.1f, k_visibleDebugColor, k_occludedDebugColor, true);
				}
			}
		}
	}
}

bool IrradianceVolumeComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<IrradianceVolumeComponent *>(instance), stream);
}

bool IrradianceVolumeComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<IrradianceVolumeComponent *>(instance), stream);
}

void IrradianceVolumeComponent::toLua(lua_State *L, void *instance) noexcept
{
	IrradianceVolumeComponent &c = *reinterpret_cast<IrradianceVolumeComponent *>(instance);

	LuaUtil::setTableIntegerField(L, "m_resolutionX", (lua_Integer)c.m_resolutionX);
	LuaUtil::setTableIntegerField(L, "m_resolutionY", (lua_Integer)c.m_resolutionY);
	LuaUtil::setTableIntegerField(L, "m_resolutionZ", (lua_Integer)c.m_resolutionZ);
	LuaUtil::setTableNumberField(L, "m_selfShadowBias", (lua_Number)c.m_selfShadowBias);
	LuaUtil::setTableNumberField(L, "m_nearPlane", (lua_Number)c.m_nearPlane);
	LuaUtil::setTableNumberField(L, "m_farPlane", (lua_Number)c.m_farPlane);
}

void IrradianceVolumeComponent::fromLua(lua_State *L, void *instance) noexcept
{
	IrradianceVolumeComponent &c = *reinterpret_cast<IrradianceVolumeComponent *>(instance);

	c.m_resolutionX = (uint32_t)LuaUtil::getTableIntegerField(L, "m_resolutionX");
	c.m_resolutionY = (uint32_t)LuaUtil::getTableIntegerField(L, "m_resolutionY");
	c.m_resolutionZ = (uint32_t)LuaUtil::getTableIntegerField(L, "m_resolutionZ");
	c.m_selfShadowBias = (float)LuaUtil::getTableNumberField(L, "m_selfShadowBias");
	c.m_nearPlane = (float)LuaUtil::getTableNumberField(L, "m_nearPlane");
	c.m_farPlane = (float)LuaUtil::getTableNumberField(L, "m_farPlane");
}
