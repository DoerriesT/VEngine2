#include "SkinnedMeshComponent.h"
#include "reflection/Reflection.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void SkinnedMeshComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<SkinnedMeshComponent>("Skinned Mesh Component");
}

void SkinnedMeshComponent::onGUI(void *instance) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);

	ImGui::LabelText("Skinned Mesh Asset", c.m_mesh->getAssetID().m_string);
	ImGui::LabelText("Skeleton Asset", c.m_skeleton->getAssetID().m_string);
}
