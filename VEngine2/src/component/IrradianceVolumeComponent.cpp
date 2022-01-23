#include "IrradianceVolumeComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"
#include "utility/Serialization.h"

template<typename Stream>
static bool serialize(IrradianceVolumeComponent &c, Stream &stream) noexcept
{
	serializeUInt32(stream, c.m_resolutionX);
	serializeUInt32(stream, c.m_resolutionY);
	serializeUInt32(stream, c.m_resolutionZ);
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

	ImGui::DragFloat("Near Plane", &c.m_nearPlane, 0.05f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the probes near plane when baking.");

	ImGui::DragFloat("Far Plane", &c.m_farPlane, 0.1f, 0.05f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGuiHelpers::Tooltip("The distance of the probes far plane when baking.");
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
	LuaUtil::setTableNumberField(L, "m_nearPlane", (lua_Number)c.m_nearPlane);
	LuaUtil::setTableNumberField(L, "m_farPlane", (lua_Number)c.m_farPlane);
	LuaUtil::setTableBoolField(L, "m_bake", c.m_bake);
}

void IrradianceVolumeComponent::fromLua(lua_State *L, void *instance) noexcept
{
	IrradianceVolumeComponent &c = *reinterpret_cast<IrradianceVolumeComponent *>(instance);

	c.m_resolutionX = (uint32_t)LuaUtil::getTableIntegerField(L, "m_resolutionX");
	c.m_resolutionY = (uint32_t)LuaUtil::getTableIntegerField(L, "m_resolutionY");
	c.m_resolutionZ = (uint32_t)LuaUtil::getTableIntegerField(L, "m_resolutionZ");
	c.m_nearPlane = (float)LuaUtil::getTableNumberField(L, "m_nearPlane");
	c.m_farPlane = (float)LuaUtil::getTableNumberField(L, "m_farPlane");
	c.m_bake = LuaUtil::getTableBoolField(L, "m_bake");
}
