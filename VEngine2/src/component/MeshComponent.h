#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"
#include "ecs/ECSCommon.h"

struct lua_State;
struct TransformComponent;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct MeshComponent
{
	Asset<MeshAsset> m_mesh;
	float m_boundingSphereSizeFactor = 1.0f;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "MeshComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Mesh Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};