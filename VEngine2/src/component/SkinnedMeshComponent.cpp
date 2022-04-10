#include "SkinnedMeshComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "asset/AssetManager.h"
#include "utility/Serialization.h"
#include "utility/Memory.h"
#include "script/LuaUtil.h"

template<typename Stream>
static bool serialize(SkinnedMeshComponent &c, Stream &stream) noexcept
{
	serializeAsset(stream, c.m_mesh, MeshAsset);
	serializeAsset(stream, c.m_skeleton, SkeletonAsset);
	serializeAsset(stream, c.m_animationGraph, AnimationGraphAsset);
	serializeFloat(stream, c.m_boundingSphereSizeFactor);

	return true;
}

void SkinnedMeshComponent::onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);

	AssetID resultAssetID;

	if (ImGuiHelpers::AssetPicker("Mesh Asset", MeshAsset::k_assetType, c.m_mesh.get(), &resultAssetID))
	{
		c.m_mesh = AssetManager::get()->getAsset<MeshAsset>(resultAssetID);
	}

	if (ImGuiHelpers::AssetPicker("Skeleton Asset", SkeletonAsset::k_assetType, c.m_skeleton.get(), &resultAssetID))
	{
		c.m_skeleton = AssetManager::get()->getAsset<SkeletonAsset>(resultAssetID);
	}

	if (ImGuiHelpers::AssetPicker("Animation Graph Asset", AnimationGraphAsset::k_assetType, c.m_animationGraph.get(), &resultAssetID))
	{
		c.m_animationGraph = AssetManager::get()->getAsset<AnimationGraphAsset>(resultAssetID);
		c.m_animationGraphInstance = AnimationGraphInstance(c.m_animationGraph);
	}

	ImGui::DragFloat("Bounding Sphere Size Factor", &c.m_boundingSphereSizeFactor, 0.01f, 0.0f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
}

bool SkinnedMeshComponent::onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept
{
	return serialize(*reinterpret_cast<SkinnedMeshComponent *>(instance), stream);
}

bool SkinnedMeshComponent::onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept
{
	return serialize(*reinterpret_cast<SkinnedMeshComponent *>(instance), stream);
}

void SkinnedMeshComponent::toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);
	LuaUtil::setTableNumberField(L, "m_boundingSphereSizeFactor", (lua_Number)c.m_boundingSphereSizeFactor);
}

void SkinnedMeshComponent::fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept
{
	SkinnedMeshComponent &c = *reinterpret_cast<SkinnedMeshComponent *>(instance);
	c.m_boundingSphereSizeFactor = (float)LuaUtil::getTableNumberField(L, "m_boundingSphereSizeFactor");
}
