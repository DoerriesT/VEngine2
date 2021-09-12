#include "MeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void MeshComponent::onGUI(void *instance) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);

	AssetData *resultAssetData = nullptr;
	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAssetData::k_assetType, c.m_mesh.get(), &resultAssetData))
	{
		c.m_mesh = resultAssetData;
	}
}
