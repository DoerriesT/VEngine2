#include "Level.h"
#include "utility/Serialization.h"
#include "ecs/ECSComponentInfoTable.h"
#include "filesystem/VirtualFileSystem.h"
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>

Level::Level(ECS *ecs) noexcept
	:m_ecs(ecs),
	m_sceneGraphNodeMemoryPool(32)
{
}

bool Level::save(const char *path) const noexcept
{
	SerializationWriteStream writeStream;

	uint32_t componentTypeCount = (uint32_t)(ECSComponentInfoTable::getHighestRegisteredComponentIDValue() + 1);
	serializeUInt32(writeStream, componentTypeCount);
	for (size_t i = 0; i < componentTypeCount; ++i)
	{
		const char *str = ECSComponentInfoTable::getComponentInfo(i).m_name;
		uint32_t strLen = (uint32_t)strlen(str) + 1;
		serializeUInt32(writeStream, strLen);
		serializeBytes(writeStream, strLen, str);
	}

	uint32_t nodeCount = (uint32_t)m_sceneGraphNodes.size();
	serializeUInt32(writeStream, nodeCount);

	for (auto *node : m_sceneGraphNodes)
	{
		// entity name
		serializeBytes(writeStream, SceneGraphNode::k_maxNameLength, node->m_name);
		
		auto compMask = m_ecs->getComponentMask(node->m_entity);
		
		// number of components
		uint32_t compCount = (uint32_t)compMask.count();
		serializeUInt32(writeStream, compCount);

		// serialize component IDs
		{
			auto componentID = compMask.DoFindFirst();
			while (componentID != compMask.kSize)
			{
				// write component ID
				serializeUInt32(writeStream, (uint32_t)componentID);

				componentID = compMask.DoFindNext(componentID);
			}
		}

		// serialize components
		{
			auto componentID = compMask.DoFindFirst();
			while (componentID != compMask.kSize)
			{
				// write component data
				const auto &compInfo = ECSComponentInfoTable::getComponentInfo(componentID);
				compInfo.m_onSerialize(m_ecs->getComponentTypeless(node->m_entity, componentID), writeStream);

				componentID = compMask.DoFindNext(componentID);
			}
		}
		
	}

	return VirtualFileSystem::get().writeFile(path, writeStream.getData().size(), writeStream.getData().data(), true);
}

bool Level::load(const char *path) noexcept
{
	if (!VirtualFileSystem::get().exists(path))
	{
		return false;
	}
	auto fileSize = VirtualFileSystem::get().size(path);
	eastl::vector<char> fileData(fileSize);
	if (!VirtualFileSystem::get().readFile(path, fileData.size(), fileData.data(), true))
	{
		return false;
	}

	SerializationReadStream readStream(fileData.size(), fileData.data());

	// get number of component types in serialized file
	uint32_t componentTypeCount = 0;
	serializeUInt32(readStream, componentTypeCount);
	if (componentTypeCount != (uint32_t)(ECSComponentInfoTable::getHighestRegisteredComponentIDValue() + 1))
	{
		return false;
	}

	// build a mapping table from serialized component IDs to runtime component IDs
	uint32_t *componentIDMappingTable = ALLOC_A_T(uint32_t, componentTypeCount);
	for (size_t i = 0; i < componentTypeCount; ++i)
	{
		uint32_t strLen = 0;
		serializeUInt32(readStream, strLen);
		eastl::fixed_string<char, 256> str;
		str.resize(strLen);
		serializeBytes(readStream, strLen, str.data());

		bool found = false;
		size_t index = -1;
		for (size_t j = 0; j < componentTypeCount; ++j)
		{
			if (strcmp(str.c_str(), ECSComponentInfoTable::getComponentInfo(j).m_name) == 0)
			{
				found = true;
				index = j;
				break;
			}
		}

		if (!found)
		{
			return false;
		}

		componentIDMappingTable[i] = index;
	}

	uint32_t nodeCount = 0;
	serializeUInt32(readStream, nodeCount);

	for (auto *node : m_sceneGraphNodes)
	{
		m_ecs->destroyEntity(node->m_entity);
	}
	m_sceneGraphNodes.clear();
	m_sceneGraphNodeMemoryPool.clear();
	m_sceneGraphNodes.reserve(nodeCount);

	for (size_t i = 0; i < nodeCount; ++i)
	{
		SceneGraphNode node{};

		// entity name
		serializeBytes(readStream, SceneGraphNode::k_maxNameLength, node.m_name);

		// number of components
		uint32_t compCount = 0;
		serializeUInt32(readStream, compCount);

		// component IDs
		eastl::fixed_vector<ComponentID, k_ecsMaxComponentTypes> componentIDs;
		for (size_t j = 0; j < compCount; ++j)
		{
			uint32_t componentID = 0;
			serializeUInt32(readStream, componentID);
			componentIDs.push_back((ComponentID)componentIDMappingTable[componentID]);
		}

		node.m_entity = m_ecs->createEntityTypeless(componentIDs.size(), componentIDs.data());

		// components
		for (auto compID : componentIDs)
		{
			const auto &compInfo = ECSComponentInfoTable::getComponentInfo(compID);
			compInfo.m_onDeserialize(m_ecs->getComponentTypeless(node.m_entity, compID), readStream);
		}

		m_sceneGraphNodes.push_back(new (m_sceneGraphNodeMemoryPool.alloc()) SceneGraphNode(node));
	}

	return true;
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
	m_sceneGraphNodes.erase(eastl::remove_if(m_sceneGraphNodes.begin(), m_sceneGraphNodes.end(), [&](const auto *e)
		{
			return e->m_entity == entity;
		}), m_sceneGraphNodes.end());
}

const eastl::vector<SceneGraphNode *> &Level::getSceneGraphNodes() noexcept
{
	return m_sceneGraphNodes;
}
