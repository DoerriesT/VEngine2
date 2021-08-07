#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"
#include "asset/SkeletonAsset.h"
#include "asset/AnimationClipAsset.h"
#include "reflection/TypeInfo.h"
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>

class Reflection;

struct SkinnedMeshComponent
{
	Asset<MeshAssetData> m_mesh;
	Asset<SkeletonAssetData> m_skeleton;
	Asset<AnimationClipAssetData> m_animationClip;
	float m_time;
	bool m_playing;
	bool m_loop;
	eastl::vector<glm::mat4> m_matrixPalette;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(SkinnedMeshComponent, "B1321A20-9DC6-40DE-9CC4-C4460BF0230D")