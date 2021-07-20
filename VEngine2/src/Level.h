#pragma once
#include <EASTL/vector.h>
#include <utility/ObjectPool.h>
#include "ecs/ECS.h"

struct SceneGraphNode
{
	static constexpr size_t k_maxNameLength = 256;
	char m_name[k_maxNameLength] = {};
	EntityID m_entity;
};

class Level
{
public:
	explicit Level() noexcept;
	void addEntity(EntityID entity, const char *name) noexcept;
	void removeEntity(EntityID entity) noexcept;
	const eastl::vector<SceneGraphNode *> &getSceneGraphNodes() noexcept;

private:
	DynamicObjectMemoryPool<SceneGraphNode> m_sceneGraphNodeMemoryPool;
	eastl::vector<SceneGraphNode *> m_sceneGraphNodes;
};