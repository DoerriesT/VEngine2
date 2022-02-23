#pragma once
#include <glm/mat4x4.hpp>
#include "ecs/ECSCommon.h"

struct lua_State;
struct TransformComponent;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct CameraComponent
{
	float m_aspectRatio = 1.0f;
	float m_fovy = 1.57079632679f; // 90 degrees
	float m_near = 0.1f;
	float m_far = 300.0f;
	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "CameraComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Camera Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};