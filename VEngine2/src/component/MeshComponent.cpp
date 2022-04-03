#include "MeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "utility/Serialization.h"
#include "utility/Memory.h"
#include "script/LuaUtil.h"

template<typename Stream>
static bool serialize(MeshComponent &c, Stream &stream) noexcept
{
	serializeAsset(stream, c.m_mesh, MeshAsset);
	serializeFloat(stream, c.m_boundingSphereSizeFactor);

	return true;
}

void MeshComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);

	AssetID resultAssetID;
	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAsset::k_assetType, c.m_mesh.get(), &resultAssetID))
	{
		c.m_mesh = AssetManager::get()->getAsset<MeshAsset>(resultAssetID);
	}

	ImGui::DragFloat("Bounding Sphere Size Factor", &c.m_boundingSphereSizeFactor, 0.01f, 0.0f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
}

bool MeshComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<MeshComponent *>(instance), stream);
}

bool MeshComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<MeshComponent *>(instance), stream);
}

void MeshComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);
	LuaUtil::setTableNumberField(L, "m_boundingSphereSizeFactor", (lua_Number)c.m_boundingSphereSizeFactor);
}

void MeshComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);
	c.m_boundingSphereSizeFactor = (float)LuaUtil::getTableNumberField(L, "m_boundingSphereSizeFactor");
}
