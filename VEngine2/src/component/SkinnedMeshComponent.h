#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"
#include "asset/SkeletonAsset.h"
#include "asset/AnimationClipAsset.h"
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>

class AnimationGraph;

struct SkinnedMeshComponent
{
	Asset<MeshAssetData> m_mesh;
	Asset<SkeletonAssetData> m_skeleton;
	AnimationGraph *m_animationGraph;
	eastl::vector<glm::mat4> m_matrixPalette;

	static void onGUI(void *instance) noexcept;
	static const char *getComponentName() noexcept { return "SkinnedMeshComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Skinned Mesh Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};