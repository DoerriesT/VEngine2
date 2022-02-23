#pragma once
#include <stdint.h>
#include "ecs/ECSCommon.h"

struct lua_State;
class ECS;
struct TransformComponent;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct IrradianceVolumeComponent
{
	uint32_t m_resolutionX = 4;
	uint32_t m_resolutionY = 4;
	uint32_t m_resolutionZ = 4;
	float m_fadeoutStart = 0.0f;
	float m_fadeoutEnd = 0.5f;
	float m_selfShadowBias = 0.3f;
	float m_nearPlane = 0.1f;
	float m_farPlane = 50.0f;
	uint32_t m_internalHandle = 0;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "IrradianceVolumeComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Irradiance Volume Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};