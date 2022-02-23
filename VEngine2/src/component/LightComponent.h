#pragma once
#include <stdint.h>
#include "ecs/ECSCommon.h"

struct lua_State;
struct TransformComponent;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct LightComponent
{
	static size_t constexpr k_maxCascades = 6;

	enum class Type
	{
		Point, Spot, Directional
	};

	Type m_type = Type::Point;
	uint32_t m_color = 0xFFFFFFFF;
	float m_intensity = 1000.0f;
	bool m_shadows = false;
	float m_radius = 8.0f;
	float m_outerAngle = 0.785f;
	float m_innerAngle = 0.26f;
	float m_splitLambda = 0.9f;
	uint32_t m_cascadeCount = 4;
	float m_depthBias[k_maxCascades] = {};
	float m_normalOffsetBias[k_maxCascades] = {};
	float m_maxShadowDistance = 100.0f;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "LightComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Light Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};