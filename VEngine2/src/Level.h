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
	explicit Level(ECS *ecs) noexcept;
	bool save(const char *path) const noexcept;
	bool load(const char *path) noexcept;
	void addEntity(EntityID entity, const char *name) noexcept;
	void removeEntity(EntityID entity) noexcept;
	const eastl::vector<SceneGraphNode *> &getSceneGraphNodes() noexcept;

private:
	ECS *m_ecs;
	DynamicObjectMemoryPool<SceneGraphNode> m_sceneGraphNodeMemoryPool;
	eastl::vector<SceneGraphNode *> m_sceneGraphNodes;
};