#include "MeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"

void MeshComponent::onGUI(void *instance) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);

	ImGui::Text("Mesh Asset");
	ImGui::BeginGroup();
	ImGui::Image(ImGui::GetIO().Fonts->TexID, ImVec2(64, 64));
	ImGui::SameLine();
	ImGui::Text(c.m_mesh->getAssetID().m_string);
	ImGui::EndGroup();

	if (ImGui::BeginDragDropTarget())
	{
		if (const auto *payload = ImGui::AcceptDragDropPayload("ASSET_ID"))
		{
			AssetID assetID = *reinterpret_cast<const AssetID *>(payload->Data);
			c.m_mesh = AssetManager::get()->getAsset<MeshAssetData>(assetID);
		}

		ImGui::EndDragDropTarget();
	}
}
