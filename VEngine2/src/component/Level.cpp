#include "..\Level.h"
#include <algorithm>

Level::Level() noexcept
	:m_sceneGraphNodeMemoryPool(32)
{
}

void Level::addEntity(EntityID entity, const char *name) noexcept
{
	SceneGraphNode node{};
	node.m_entity = entity;
	strcpy_s(node.m_name, name);

	m_sceneGraphNodes.push_back(new (m_sceneGraphNodeMemoryPool.alloc()) SceneGraphNode(node));
}

void Level::removeEntity(EntityID entity) noexcept
{
	m_sceneGraphNodes.erase(std::remove_if(m_sceneGraphNodes.begin(), m_sceneGraphNodes.end(), [&](const auto *e)
		{
			return e->m_entity == entity;
		}), m_sceneGraphNodes.end());
}

const eastl::vector<SceneGraphNode *> &Level::getSceneGraphNodes() noexcept
{
	return m_sceneGraphNodes;
}
