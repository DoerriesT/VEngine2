#include "SkinnedMeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void SkinnedMeshComponent::onGUI(void *instance) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);

	AssetData *resultAssetData = nullptr;

	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAssetData::k_assetType, c.m_mesh.get(), &resultAssetData))
	{
		c.m_mesh = resultAssetData;
	}

	if (ImGuiHelpers::AssetPicker("Skeleton Asset", SkeletonAssetData::k_assetType, c.m_skeleton.get(), &resultAssetData))
	{
		c.m_skeleton = resultAssetData;
	}
}

void SkinnedMeshComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void SkinnedMeshComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
