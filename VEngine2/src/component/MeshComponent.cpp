#include "MeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "utility/Serialization.h"
#include "utility/Memory.h"

template<typename Stream>
static bool serialize(MeshComponent &c, Stream &stream) noexcept
{
	serializeAsset(stream, c.m_mesh, MeshAssetData);

	return true;
}

void MeshComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	MeshComponent &c = *reinterpret_cast<MeshComponent *>(instance);

	AssetID resultAssetID;
	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAssetData::k_assetType, c.m_mesh.get(), &resultAssetID))
	{
		c.m_mesh = AssetManager::get()->getAsset<MeshAssetData>(resultAssetID);
	}
}

bool MeshComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<MeshComponent *>(instance), stream);
}

bool MeshComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<MeshComponent *>(instance), stream);
}

void MeshComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void MeshComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
