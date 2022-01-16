#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"

struct lua_State;
struct TransformComponent;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct MeshComponent
{
	Asset<MeshAssetData> m_mesh;

	static void onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(lua_State *L, void *instance) noexcept;
	static void fromLua(lua_State *L, void *instance) noexcept;
	static const char *getComponentName() noexcept { return "MeshComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Mesh Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};