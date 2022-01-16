#include "SkinnedMeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "utility/Serialization.h"
#include "utility/Memory.h"

template<typename Stream>
static bool serialize(SkinnedMeshComponent &c, Stream &stream) noexcept
{
	serializeAsset(stream, c.m_mesh, MeshAssetData);
	serializeAsset(stream, c.m_skeleton, SkeletonAssetData);

	return true;
}

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

bool SkinnedMeshComponent::onSerialize(void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<SkinnedMeshComponent *>(instance), stream);
}

bool SkinnedMeshComponent::onDeserialize(void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<SkinnedMeshComponent *>(instance), stream);
}

void SkinnedMeshComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void SkinnedMeshComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
