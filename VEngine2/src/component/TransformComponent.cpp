#include "TransformComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"
#include "utility/Serialization.h"
#include "TransformHierarchy.h"
#include "ecs/ECS.h"

template<typename Stream>
static bool serialize(TransformComponent &c, Stream &stream) noexcept
{
	uint32_t mobilityInt = static_cast<uint32_t>(c.m_mobility);
	serializeUInt32(stream, mobilityInt);
	c.m_mobility = static_cast<Mobility>(mobilityInt);
	serializeFloat(stream, c.m_transform.m_translation.x);
	serializeFloat(stream, c.m_transform.m_translation.y);
	serializeFloat(stream, c.m_transform.m_translation.z);
	serializeFloat(stream, c.m_transform.m_rotation.x);
	serializeFloat(stream, c.m_transform.m_rotation.y);
	serializeFloat(stream, c.m_transform.m_rotation.z);
	serializeFloat(stream, c.m_transform.m_rotation.w);
	serializeFloat(stream, c.m_transform.m_scale.x);
	serializeFloat(stream, c.m_transform.m_scale.y);
	serializeFloat(stream, c.m_transform.m_scale.z);

	return true;
}

void TransformComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	int mobilityInt = static_cast<int>(c.m_mobility);
	ImGui::RadioButton("Static", &mobilityInt, static_cast<int>(Mobility::Static)); ImGui::SameLine();
	ImGui::RadioButton("Dynamic", &mobilityInt, static_cast<int>(Mobility::Dynamic));
	auto newMobility = static_cast<Mobility>(mobilityInt);
	if (newMobility != c.m_mobility)
	{
		TransformHierarchy::setMobility(ecs, entity, newMobility, &c);
	}

	bool transformChanged = false;

	Transform transform = c.m_transform;

	// translation
	transformChanged |= ImGui::DragFloat3("Translation", &transform.m_translation.x, 0.1f);

	// rotation
	{
		glm::vec3 eulerAnglesBefore = glm::degrees(glm::eulerAngles(transform.m_rotation));
		glm::vec3 eulerAngles = eulerAnglesBefore;
		if (ImGui::DragFloat3("Rotation", &eulerAngles.x, 1.0f, -360.0f, 360.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
		{
			glm::vec3 diff = eulerAngles - eulerAnglesBefore;
			glm::quat diffQuat = glm::quat(glm::radians(diff));

			transform.m_rotation = glm::normalize(transform.m_rotation * diffQuat);
			transformChanged = true;
		}
	}

	// scale
	transformChanged |= ImGui::DragFloat3("Scale", &c.m_transform.m_scale.x, 0.1f);

	if (transformChanged)
	{
		TransformHierarchy::setLocalTransform(ecs, entity, transform, &c);
	}
}

bool TransformComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<TransformComponent *>(instance), stream);
}

bool TransformComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<TransformComponent *>(instance), stream);
}

void TransformComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	LuaUtil::setTableIntegerField(L, "m_mobility", (lua_Integer)c.m_mobility);
	LuaUtil::setTableNumberField(L, "m_translationX", (lua_Number)c.m_transform.m_translation.x);
	LuaUtil::setTableNumberField(L, "m_translationY", (lua_Number)c.m_transform.m_translation.y);
	LuaUtil::setTableNumberField(L, "m_translationZ", (lua_Number)c.m_transform.m_translation.z);
	LuaUtil::setTableNumberField(L, "m_rotationX", (lua_Number)c.m_transform.m_rotation.x);
	LuaUtil::setTableNumberField(L, "m_rotationY", (lua_Number)c.m_transform.m_rotation.y);
	LuaUtil::setTableNumberField(L, "m_rotationZ", (lua_Number)c.m_transform.m_rotation.z);
	LuaUtil::setTableNumberField(L, "m_rotationW", (lua_Number)c.m_transform.m_rotation.w);
	LuaUtil::setTableNumberField(L, "m_scaleX", (lua_Number)c.m_transform.m_scale.x);
	LuaUtil::setTableNumberField(L, "m_scaleY", (lua_Number)c.m_transform.m_scale.y);
	LuaUtil::setTableNumberField(L, "m_scaleZ", (lua_Number)c.m_transform.m_scale.z);
}

void TransformComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	TransformComponent &c = *reinterpret_cast<TransformComponent *>(instance);

	auto newMobility = (Mobility)LuaUtil::getTableIntegerField(L, "m_mobility");
	if (newMobility != c.m_mobility)
	{
		TransformHierarchy::setMobility(ecs, entity, newMobility, &c);
	}

	Transform newTransform;
	newTransform.m_translation.x = (float)LuaUtil::getTableNumberField(L, "m_translationX");
	newTransform.m_translation.y = (float)LuaUtil::getTableNumberField(L, "m_translationY");
	newTransform.m_translation.z = (float)LuaUtil::getTableNumberField(L, "m_translationZ");
	newTransform.m_rotation.x = (float)LuaUtil::getTableNumberField(L, "m_rotationX");
	newTransform.m_rotation.y = (float)LuaUtil::getTableNumberField(L, "m_rotationY");
	newTransform.m_rotation.z = (float)LuaUtil::getTableNumberField(L, "m_rotationZ");
	newTransform.m_rotation.w = (float)LuaUtil::getTableNumberField(L, "m_rotationW");
	newTransform.m_scale.x = (float)LuaUtil::getTableNumberField(L, "m_scaleX");
	newTransform.m_scale.y = (float)LuaUtil::getTableNumberField(L, "m_scaleY");
	newTransform.m_scale.z = (float)LuaUtil::getTableNumberField(L, "m_scaleZ");

	if (memcmp(&newTransform, &c.m_transform, sizeof(newTransform)) != 0)
	{
		TransformHierarchy::setLocalTransform(ecs, entity, newTransform, &c);
	}
}
