#include "MeshComponent.h"
#include "reflection/Reflection.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"

void MeshComponent::reflect(Reflection &refl) noexcept
{
	refl.addClass<MeshComponent>("Mesh Component");
}

void MeshComponent::onGUI(void *instance) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);

	ImGui::LabelText("Mesh Asset", c.m_mesh->getAssetID().m_string);
}
