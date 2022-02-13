#include "SkinnedMeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "utility/Serialization.h"
#include "utility/Memory.h"
#include "script/LuaUtil.h"

template<typename Stream>
static bool serialize(SkinnedMeshComponent &c, Stream &stream) noexcept
{
	serializeAsset(stream, c.m_mesh, MeshAssetData);
	serializeAsset(stream, c.m_skeleton, SkeletonAssetData);
	serializeFloat(stream, c.m_boundingSphereSizeFactor);

	return true;
}

void SkinnedMeshComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);

	AssetID resultAssetID;

	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAssetData::k_assetType, c.m_mesh.get(), &resultAssetID))
	{
		c.m_mesh = AssetManager::get()->getAsset<MeshAssetData>(resultAssetID);
	}

	if (ImGuiHelpers::AssetPicker("Skeleton Asset", SkeletonAssetData::k_assetType, c.m_skeleton.get(), &resultAssetID))
	{
		c.m_skeleton = AssetManager::get()->getAsset<SkeletonAssetData>(resultAssetID);
	}

	ImGui::DragFloat("Bounding Sphere Size Factor", &c.m_boundingSphereSizeFactor, 0.01f, 0.0f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
}

bool SkinnedMeshComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<SkinnedMeshComponent *>(instance), stream);
}

bool SkinnedMeshComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<SkinnedMeshComponent *>(instance), stream);
}

void SkinnedMeshComponent::toLua(lua_State *L, void *instance) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);
	LuaUtil::setTableNumberField(L, "m_boundingSphereSizeFactor", (lua_Number)c.m_boundingSphereSizeFactor);
}

void SkinnedMeshComponent::fromLua(lua_State *L, void *instance) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);
	c.m_boundingSphereSizeFactor = (float)LuaUtil::getTableNumberField(L, "m_boundingSphereSizeFactor");
}
