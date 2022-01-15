#include "SkinnedMeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"

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
}

void SkinnedMeshComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void SkinnedMeshComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
