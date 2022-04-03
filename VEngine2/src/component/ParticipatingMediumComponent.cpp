#include "ParticipatingMediumComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "script/LuaUtil.h"
#include "utility/Serialization.h"
#include "utility/Memory.h"

template<typename Stream>
static bool serialize(ParticipatingMediumComponent &c, Stream &stream) noexcept
{
	uint32_t typeInt = static_cast<uint32_t>(c.m_type);
	serializeUInt32(stream, typeInt);
	c.m_type = static_cast<ParticipatingMediumComponent::Type>(typeInt);
	serializeUInt32(stream, c.m_albedo);
	serializeFloat(stream, c.m_extinction);
	serializeUInt32(stream, c.m_emissiveColor);
	serializeFloat(stream, c.m_emissiveIntensity);
	serializeFloat(stream, c.m_phaseAnisotropy);
	serializeBool(stream, c.m_heightFogEnabled);
	serializeFloat(stream, c.m_heightFogStart);
	serializeFloat(stream, c.m_heightFogFalloff);
	serializeFloat(stream, c.m_maxHeight);
	serializeAsset(stream, c.m_densityTexture, TextureAsset);
	serializeFloat(stream, c.m_textureScale);
	serializeFloat(stream, c.m_textureBias[0]);
	serializeFloat(stream, c.m_textureBias[1]);
	serializeFloat(stream, c.m_textureBias[2]);
	serializeBool(stream, c.m_spherical);

	return true;
}

void ParticipatingMediumComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
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
	if (ImGuiHelpers::AssetPicker("Density Texture", TextureAsset::k_assetType, c.m_densityTexture.get(), &resultAssetID))
	{
		c.m_densityTexture = AssetManager::get()->getAsset<TextureAsset>(resultAssetID);
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

bool ParticipatingMediumComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<ParticipatingMediumComponent *>(instance), stream);
}

bool ParticipatingMediumComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<ParticipatingMediumComponent *>(instance), stream);
}

void ParticipatingMediumComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}

void ParticipatingMediumComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
}
