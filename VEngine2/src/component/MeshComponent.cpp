#include "MeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"

void MeshComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);

	AssetID resultAssetID;
	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAssetData::k_assetType, c.m_mesh.get(), &resultAssetID))
	{
		c.m_mesh = AssetManager::get()->getAsset<MeshAssetData>(resultAssetID);
	}
}

void MeshComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void MeshComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
