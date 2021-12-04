#include "ParticipatingMediumComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "script/LuaUtil.h"

void ParticipatingMediumComponent::onGUI(void *instance) noexcept
{
	ParticipatingMediumComponent &c = *reinterpret_cast<ParticipatingMediumComponent *>(instance);

	int typeInt = static_cast<int>(c.m_type);
	ImGui::RadioButton("Global", &typeInt, static_cast<int>(ParticipatingMediumComponent::Type::Global)); ImGui::SameLine();
	ImGui::RadioButton("Local", &typeInt, static_cast<int>(ParticipatingMediumComponent::Type::Local));
	c.m_type = static_cast<ParticipatingMediumComponent::Type>(typeInt);

	ImGuiHelpers::ColorEdit3("Albedo", &c.m_albedo);
	ImGui::DragFloat("Extinction", &c.m_extinction, 0.001f, 0.0f, FLT_MAX, "%.7f");
	ImGuiHelpers::ColorEdit3("Emissive Color", &c.m_emissiveColor);
	ImGui::DragFloat("Emissive Intensity", &c.m_emissiveIntensity, 0.001f, 0.0f, FLT_MAX, "%.7f");
	ImGui::DragFloat("Phase", &c.m_phaseAnisotropy, 0.001f, -0.9f, 0.9f, "%.7f");
	ImGui::Checkbox("Height Fog", &c.m_heightFogEnabled);
	ImGui::DragFloat("Height Fog Start", &c.m_heightFogStart, 0.1f);
	ImGui::DragFloat("Height Fog Falloff", &c.m_heightFogFalloff, 0.1f);
	ImGui::DragFloat("Max Height", &c.m_maxHeight, 0.1f);

	AssetID resultAssetID;
	if (ImGuiHelpers::AssetPicker("Density Texture", TextureAssetData::k_assetType, c.m_densityTexture.get(), &resultAssetID))
	{
		c.m_densityTexture = AssetManager::get()->getAsset<TextureAssetData>(resultAssetID);
	}

	ImGui::DragFloat("Density Texture Scale", &c.m_textureScale, 0.1f, 0.0f, FLT_MAX);
	ImGui::DragFloat3("Density Texture Bias", c.m_textureBias, 0.1f);

	if (c.m_type == ParticipatingMediumComponent::Type::Local)
	{
		int sphericalInt = static_cast<int>(c.m_spherical);
		ImGui::RadioButton("Box", &sphericalInt, 0); ImGui::SameLine();
		ImGui::RadioButton("Sphere", &sphericalInt, 1); ImGui::SameLine();
		c.m_spherical = sphericalInt != 0;
	}
}

void ParticipatingMediumComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void ParticipatingMediumComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
