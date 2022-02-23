#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ecs/ECSCommon.h"
#include "utility/Transform.h"

struct lua_State;
class ECS;
class Renderer;
class SerializationWriteStream;
class SerializationReadStream;

struct TransformComponent
{
public:
	Mobility m_mobility = Mobility::Static; // use TransformHierarchy::setMobility() to change
	Transform m_transform; // use TransformHierarchy::setLocalTransform() to change
	Transform m_globalTransform; // use TransformHierarchy::setGlobalTransform() to change
	Transform m_prevGlobalTransform;
	Transform m_curRenderTransform;
	Transform m_prevRenderTransform;
	EntityID m_parentEntity = {}; // DO NOT TOUCH! use TransformHierarchy::attach() and TransformHierarchy::detach() instead!
	EntityID m_prevSiblingEntity = {}; // DO NOT TOUCH! use TransformHierarchy::attach() and TransformHierarchy::detach() instead!
	EntityID m_nextSiblingEntity = {}; // DO NOT TOUCH! use TransformHierarchy::attach() and TransformHierarchy::detach() instead!
	EntityID m_childEntity = {}; // DO NOT TOUCH! use TransformHierarchy::attach() and TransformHierarchy::detach() instead!

	TransformComponent() noexcept = default;
	explicit TransformComponent(const Transform &transform, Mobility mobility = Mobility::Static)
		:m_mobility(mobility),
		m_transform(transform),
		m_globalTransform(transform),
		m_prevGlobalTransform(transform),
		m_curRenderTransform(transform),
		m_prevRenderTransform(transform)
	{
	}

	static void onGUI(ECS *ecs, EntityID entity, void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	static bool onSerialize(ECS *ecs, EntityID entity, void *instance, SerializationWriteStream &stream) noexcept;
	static bool onDeserialize(ECS *ecs, EntityID entity, void *instance, SerializationReadStream &stream) noexcept;
	static void toLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static void fromLua(ECS *ecs, EntityID entity, void *instance, lua_State *L) noexcept;
	static const char *getComponentName() noexcept { return "TransformComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Transform Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};