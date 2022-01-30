#include "ReflectionProbeComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include "TransformComponent.h"
#include "graphics/Renderer.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(ReflectionProbeComponent &c, Stream &stream) noexcept
{
	serializeFloat(stream, c.m_captureOffset.x);
	serializeFloat(stream, c.m_captureOffset.y);
	serializeFloat(stream, c.m_captureOffset.z);
	serializeFloat(stream, c.m_nearPlane);
	serializeFloat(stream, c.m_farPlane);
	for (auto &d : c.m_boxFadeDistances)
	{
		serializeFloat(stream, d);
	}
	serializeBool(stream, c.m_lockedFadeDistance);

	return true;
}

void ReflectionProbeComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	ReflectionProbeComponent &c = *reinterpret_cast<ReflectionProbeComponent *>(instance);

	ImGui::DragFloat3("Capture Offset", &c.m_captureOffset.x, 0.1f);
	ImGui::DragFloat("Near Plane##LRP", &c.m_nearPlane, 0.05f, 0.05f, c.m_farPlane);
	ImGui::DragFloat("Far Plane##LRP", &c.m_farPlane, 0.25f, c.m_nearPlane, FLT_MAX);

	const char *k_fadeDistStrings[]
	{
		"Box Fade Distance  X",
		"Box Fade Distance -X",
		"Box Fade Distance  Y",
		"Box Fade Distance -Y",
		"Box Fade Distance  Z",
		"Box Fade Distance -Z",
	};

	for (size_t i = 0; i < 6; ++i)
	{
		if (ImGui::DragFloat(k_fadeDistStrings[i], &c.m_boxFadeDistances[i], 0.05f, 0.0f, 100.0f) && c.m_lockedFadeDistance)
		{
			for (size_t j = 0; j < 6; ++j)
			{
				c.m_boxFadeDistances[j] = c.m_boxFadeDistances[i];
			}
		}
	}

	ImGui::Checkbox("Locked Fade Distance", &c.m_lockedFadeDistance);

	if (ImGui::Button("Refresh"))
	{
		c.m_recapture = true;
	}

	if (renderer && transformComponent)
	{
		const glm::vec4 k_visibleDebugColor = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
		const glm::vec4 k_occludedDebugColor = glm::vec4(0.5f, 0.25f, 0.0f, 1.0f);

		auto &transform = transformComponent->m_transform;
		glm::mat4 boxTransform = glm::translate(transform.m_translation) * glm::mat4_cast(transform.m_rotation) * glm::scale(transform.m_scale);
		renderer->drawDebugBox(boxTransform, k_visibleDebugColor, k_occludedDebugColor, true);
		renderer->drawDebugBox(boxTransform, glm::vec4(1.0f, 0.5f, 0.0f, 0.125f), glm::vec4(0.5f, 0.25f, 0.0f, 0.125f), true, false);

		glm::vec3 fadeMinCorner = -transform.m_scale + glm::vec3(c.m_boxFadeDistances[1], c.m_boxFadeDistances[3], c.m_boxFadeDistances[5]);
		glm::vec3 fadeMaxCorner = transform.m_scale - glm::vec3(c.m_boxFadeDistances[0], c.m_boxFadeDistances[2], c.m_boxFadeDistances[4]);

		glm::vec3 fadeDistCenter = (fadeMinCorner + fadeMaxCorner) * 0.5f;
		glm::vec3 fadeDistExtent = fadeMaxCorner - fadeDistCenter;

		fadeDistCenter += transform.m_translation;

		boxTransform = glm::translate(fadeDistCenter) * glm::mat4_cast(transform.m_rotation) * glm::scale(fadeDistExtent);
		renderer->drawDebugBox(boxTransform, k_visibleDebugColor, k_occludedDebugColor, true);

		glm::vec3 capturePosition = transform.m_translation + c.m_captureOffset;
		renderer->drawDebugCross(capturePosition, 0.2f, k_visibleDebugColor, k_occludedDebugColor, true);
	}
}

bool ReflectionProbeComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<ReflectionProbeComponent *>(instance), stream);
}

bool ReflectionProbeComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<ReflectionProbeComponent *>(instance), stream);
}

void ReflectionProbeComponent::toLua(lua_State *L, void *instance) noexcept
{
	ReflectionProbeComponent &c = *reinterpret_cast<ReflectionProbeComponent *>(instance);

	LuaUtil::setTableNumberField(L, "m_captureOffsetX", (lua_Number)c.m_captureOffset.x);
	LuaUtil::setTableNumberField(L, "m_captureOffsetY", (lua_Number)c.m_captureOffset.y);
	LuaUtil::setTableNumberField(L, "m_captureOffsetZ", (lua_Number)c.m_captureOffset.z);
	LuaUtil::setTableNumberField(L, "m_nearPlane", (lua_Number)c.m_nearPlane);
	LuaUtil::setTableNumberField(L, "m_farPlane", (lua_Number)c.m_farPlane);
	LuaUtil::setTableNumberArrayField(L, "m_boxFadeDistances", c.m_boxFadeDistances, 6);
	LuaUtil::setTableBoolField(L, "m_lockedFadeDistance", c.m_lockedFadeDistance);
	LuaUtil::setTableBoolField(L, "m_recapture", c.m_recapture);
}

void ReflectionProbeComponent::fromLua(lua_State *L, void *instance) noexcept
{
	ReflectionProbeComponent &c = *reinterpret_cast<ReflectionProbeComponent *>(instance);

	c.m_captureOffset.x = (float)LuaUtil::getTableNumberField(L, "m_captureOffsetX");
	c.m_captureOffset.y = (float)LuaUtil::getTableNumberField(L, "m_captureOffsetY");
	c.m_captureOffset.z = (float)LuaUtil::getTableNumberField(L, "m_captureOffsetZ");
	c.m_nearPlane = (float)LuaUtil::getTableNumberField(L, "m_nearPlane");
	c.m_farPlane = (float)LuaUtil::getTableNumberField(L, "m_farPlane");
	LuaUtil::getTableNumberArrayField(L, "m_boxFadeDistances", 6, c.m_boxFadeDistances);
	c.m_lockedFadeDistance = LuaUtil::getTableBoolField(L, "m_lockedFadeDistance");
	c.m_recapture = LuaUtil::getTableBoolField(L, "m_recapture");
}
