#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"
#include "asset/SkeletonAsset.h"
#include "asset/AnimationClipAsset.h"
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>
#include "ecs/ECSCommon.h"

class AnimationGraph;
struct lua_State;
struct TransformComponent;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct SkinnedMeshComponent
{
	Asset<MeshAsset> m_mesh;
	Asset<SkeletonAsset> m_skeleton;
	AnimationGraph *m_animationGraph;
	eastl::vector<glm::mat4> m_matrixPalette;
	eastl::vector<glm::mat4> m_prevMatrixPalette;
	eastl::vector<glm::mat4> m_curRenderMatrixPalette;
	eastl::vector<glm::mat4> m_prevRenderMatrixPalette;
	float m_boundingSphereSizeFactor = 1.0f;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "SkinnedMeshComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Skinned Mesh Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};