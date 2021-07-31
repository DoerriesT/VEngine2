#include "ECS.h"
#include <assert.h>
#include "utility/Utility.h"

ComponentID ComponentIDGenerator::m_idCount = 0;

Archetype::Archetype(const ComponentMask &componentMask, const ErasedType *componentInfo) noexcept
	:m_componentMask(componentMask),
	m_componentInfo(componentInfo),
	m_memoryChunkSize(),
	m_memoryChunks(),
	m_componentArrayOffsets()
{
	constexpr size_t k_baseArrayOffset = sizeof(EntityID) * k_ecsComponentsPerMemoryChunk;

	// compute total chunk size and array offsets by looping over all components of this archetype
	m_memoryChunkSize += k_baseArrayOffset;

	forEachComponentType(m_componentMask, [this](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];
			m_memoryChunkSize = util::alignPow2Up<size_t>(m_memoryChunkSize, compInfo.m_alignment);
			m_componentArrayOffsets.push_back(m_memoryChunkSize);
			m_memoryChunkSize += compInfo.m_size * k_ecsComponentsPerMemoryChunk;
		});
}

size_t Archetype::getComponentArrayOffset(ComponentID componentID) noexcept
{
	size_t lastFoundComponentID = componentID;
	size_t numPrevComponents = 0;
	while (lastFoundComponentID != k_ecsMaxComponentTypes)
	{
		lastFoundComponentID = m_componentMask.DoFindPrev(lastFoundComponentID);
		++numPrevComponents;
	}

	return m_componentArrayOffsets[numPrevComponents - 1];
}

ArchetypeSlot Archetype::allocateDataSlot() noexcept
{
	ArchetypeSlot resultSlot{};

	for (auto &chunk : m_memoryChunks)
	{
		// found an existing chunk with space for our entity
		if (chunk.m_size < k_ecsComponentsPerMemoryChunk)
		{
			resultSlot.m_chunkSlotIdx = (uint32_t)chunk.m_size++;
			return resultSlot;
		}
		++resultSlot.m_chunkIdx;
	}

	// no more space in existing memory chunks -> create a new one
	ArchetypeMemoryChunk chunk{};
	chunk.m_memory = new uint8_t[m_memoryChunkSize];
	chunk.m_size = 1;

	m_memoryChunks.push_back(chunk);
	resultSlot.m_chunkSlotIdx = 0;

	return resultSlot;
}

void Archetype::freeDataSlot(const ArchetypeSlot &slot) noexcept
{
	const size_t chunkIdx = slot.m_chunkIdx;
	const size_t chunkSlotIdx = slot.m_chunkSlotIdx;

	auto &chunk = m_memoryChunks[chunkIdx];

	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];
			uint8_t *freedComp = chunk.m_memory + m_componentArrayOffsets[index] + compInfo.m_size * chunkSlotIdx;
			uint8_t *lastComp = chunk.m_memory + m_componentArrayOffsets[index] + compInfo.m_size * (chunk.m_size - 1);

			// call destructor on to be freed element
			compInfo.m_destructor(freedComp);

			if (freedComp != lastComp)
			{
				// move last element into free slot
				compInfo.m_moveConstructor(freedComp, lastComp);

				// call destructor on last element
				compInfo.m_destructor(lastComp);
			}
		});

	// swap accompanying entities
	reinterpret_cast<EntityID *>(chunk.m_memory)[chunkSlotIdx] = reinterpret_cast<EntityID *>(chunk.m_memory)[chunk.m_size - 1];

	// reduce chunk size
	--chunk.m_size;
}

EntityRecord Archetype::migrate(EntityID entity, const EntityRecord &oldRecord, bool skipConstructorOfMissingComponents) noexcept
{
	if (oldRecord.m_archetype == this)
	{
		return oldRecord;
	}

	const auto slot = allocateDataSlot();
	const size_t chunkIdx = slot.m_chunkIdx;
	const size_t chunkSlotIdx = slot.m_chunkSlotIdx;

	auto &chunk = m_memoryChunks[chunkIdx];
	ArchetypeMemoryChunk *oldChunk = nullptr;
	const size_t oldChunkSlotIdx = oldRecord.m_slot.m_chunkSlotIdx;
	if (oldRecord.m_archetype)
	{
		oldChunk = &oldRecord.m_archetype->m_memoryChunks[oldRecord.m_slot.m_chunkIdx];
	}

	// iterate over all components in this archetype and initialize them
	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];

			uint8_t *newComp = chunk.m_memory + m_componentArrayOffsets[index] + compInfo.m_size * chunkSlotIdx;

			// old archetype shares this component -> move it
			if (oldRecord.m_archetype && oldRecord.m_archetype->m_componentMask[componentID])
			{
				uint8_t *oldComp = oldChunk->m_memory + oldRecord.m_archetype->getComponentArrayOffset(componentID) + compInfo.m_size * oldChunkSlotIdx;
				compInfo.m_moveConstructor(newComp, oldComp);
			}
			// old archetype does not share this component -> default construct it
			else if (!skipConstructorOfMissingComponents)
			{
				compInfo.m_defaultConstructor(newComp);
			}
		});

	reinterpret_cast<EntityID *>(chunk.m_memory)[chunkSlotIdx] = entity;

	// free slot in old archetype
	if (oldRecord.m_archetype)
	{
		oldRecord.m_archetype->freeDataSlot(oldRecord.m_slot);
	}

	EntityRecord newRecord{};
	newRecord.m_archetype = this;
	newRecord.m_slot = slot;

	return newRecord;
}

uint8_t *Archetype::getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) noexcept
{
	if (!m_componentMask[componentID] || slot.m_chunkIdx >= m_memoryChunks.size() || slot.m_chunkSlotIdx >= m_memoryChunks[slot.m_chunkIdx].m_size)
	{
		return nullptr;
	}

	return m_memoryChunks[slot.m_chunkIdx].m_memory + getComponentArrayOffset(componentID) + m_componentInfo[componentID].m_size * slot.m_chunkSlotIdx;
}

EntityID ECS::createEntity() noexcept
{
	EntityID id = m_nextFreeEntityId++;
	m_entityRecords[id] = {};
	return id;
}

EntityID ECS::createEntityTypeless(size_t componentCount, const ComponentID *componentIDs) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	return createEntityInternal(componentCount, componentIDs, nullptr, ComponentConstructorType::DEFAULT);
}

EntityID ECS::createEntityTypeless(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	return createEntityInternal(componentCount, componentIDs, componentData, ComponentConstructorType::COPY);
}

void ECS::destroyEntity(EntityID entity) noexcept
{
	auto it = m_entityRecords.find(entity);
	if (it != m_entityRecords.end() && it->second.m_archetype)
	{
		// delete components
		it->second.m_archetype->freeDataSlot(it->second.m_slot);

		// delete entity
		m_entityRecords.erase(it);
	}
}

void ECS::addComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	addComponentsInternal(entity, componentCount, componentIDs, nullptr, ComponentConstructorType::DEFAULT);
}

void ECS::addComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	addComponentsInternal(entity, componentCount, componentIDs, componentData, ComponentConstructorType::COPY);
}

bool ECS::removeComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	return removeComponentsInternal(entity, componentCount, componentIDs);
}

void *ECS::getComponentTypeless(EntityID entity, ComponentID componentID) noexcept
{
	assert(isRegisteredComponent(1, &componentID));
	assert(isNotSingletonComponent(1, &componentID));
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];

	if (record.m_archetype)
	{
		return record.m_archetype->getComponentMemory(record.m_slot, componentID);
	}

	return nullptr;
}

bool ECS::hasComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	if (componentCount == 0)
	{
		return true;
	}

	auto record = m_entityRecords[entity];

	if (componentCount > 0 && !record.m_archetype)
	{
		return false;
	}

	for (size_t j = 0; j < componentCount; ++j)
	{
		if (!record.m_archetype->m_componentMask[componentIDs[j]])
		{
			return false;
		}
	}

	return true;
}

ComponentMask ECS::getComponentMask(EntityID entity) noexcept
{
	assert(m_entityRecords.find(entity) != m_entityRecords.end());
	auto record = m_entityRecords[entity];

	return record.m_archetype ? record.m_archetype->m_componentMask : 0;
}

ComponentMask ECS::getRegisteredComponentMask() noexcept
{
	ComponentMask mask = 0;

	for (size_t i = 0; i < mask.kSize; ++i)
	{
		mask[i] = m_componentInfo[i].m_defaultConstructor != nullptr && !m_singletonComponentsBitset[i];
	}

	return mask;
}

ComponentMask ECS::getRegisteredComponentMaskWithSingletons() noexcept
{
	ComponentMask mask = 0;

	for (size_t i = 0; i < mask.kSize; ++i)
	{
		mask[i] = m_componentInfo[i].m_defaultConstructor != nullptr;
	}

	return mask;
}

bool ECS::isRegisteredComponent(size_t count, const ComponentID *componentIDs) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		if (!m_componentInfo[componentIDs[i]].m_defaultConstructor)
		{
			return false;
		}
	}
	return true;
}

bool ECS::isNotSingletonComponent(size_t count, const ComponentID *componentIDs) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		if (m_singletonComponentsBitset[componentIDs[i]])
		{
			return false;
		}
	}
	return true;
}

EntityID ECS::createEntityInternal(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept
{
	EntityID entityID = m_nextFreeEntityId++;

	if (componentCount == 0)
	{
		m_entityRecords[entityID] = {};
		return entityID;
	}

	// build component mask
	ComponentMask compMask = 0;

	for (size_t j = 0; j < componentCount; ++j)
	{
		compMask.set(componentIDs[j], true);
	}

	// find archetype
	Archetype *archetype = findOrCreateArchetype(compMask);

	// allocate slot for entity
	auto slot = archetype->allocateDataSlot();

	switch (constructorType)
	{
	case ECS::ComponentConstructorType::DEFAULT:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			m_componentInfo[componentIDs[j]].m_defaultConstructor(archetype->getComponentMemory(slot, componentIDs[j]));
		}
		break;
	}
	case ECS::ComponentConstructorType::COPY:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			m_componentInfo[componentIDs[j]].m_copyConstructor(archetype->getComponentMemory(slot, componentIDs[j]), componentData[j]);
		}
		break;
	}
	case ECS::ComponentConstructorType::MOVE:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			// const_cast is legal assuming that componentData is an array of non-const void * in the move constructor case
			m_componentInfo[componentIDs[j]].m_moveConstructor(archetype->getComponentMemory(slot, componentIDs[j]), const_cast<void *>(componentData[j]));
		}
		break;
	}
	default:
		assert(false);
		break;
	}


	EntityRecord record{};
	record.m_archetype = archetype;
	record.m_slot = slot;

	m_entityRecords[entityID] = record;

	return entityID;
}

void ECS::addComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept
{
	assert(componentCount > 0);
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	EntityRecord &entityRecord = m_entityRecords[entity];

	// build new component mask
	ComponentMask oldMask = entityRecord.m_archetype ? entityRecord.m_archetype->m_componentMask : 0;
	ComponentMask newMask = oldMask;

	for (size_t j = 0; j < componentCount; ++j)
	{
		newMask.set(componentIDs[j], true);
	}

	const bool allComponentsPresent = oldMask == newMask;
	Archetype *newArchetype = allComponentsPresent ? entityRecord.m_archetype : findOrCreateArchetype(newMask);
	if (!allComponentsPresent)
	{
		entityRecord = newArchetype->migrate(entity, entityRecord, true);
	}

	switch (constructorType)
	{
	case ECS::ComponentConstructorType::DEFAULT:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			auto *componentMem = newArchetype->getComponentMemory(entityRecord.m_slot, componentIDs[j]);

			// component was present in old archetype and moved into new one during migration:
			// call destructor to ensure clean default construction
			if (oldMask[componentIDs[j]])
			{
				m_componentInfo[componentIDs[j]].m_destructor(componentMem);
			}
			m_componentInfo[componentIDs[j]].m_defaultConstructor(componentMem);
		}
		break;
	}
	case ECS::ComponentConstructorType::COPY:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			auto *componentMem = newArchetype->getComponentMemory(entityRecord.m_slot, componentIDs[j]);

			// component was present in old archetype and moved into new one during migration:
			// call destructor to ensure clean default construction
			if (oldMask[componentIDs[j]])
			{
				m_componentInfo[componentIDs[j]].m_destructor(componentMem);
			}
			m_componentInfo[componentIDs[j]].m_copyConstructor(componentMem, componentData[j]);
		}
		break;
	}
	case ECS::ComponentConstructorType::MOVE:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			auto *componentMem = newArchetype->getComponentMemory(entityRecord.m_slot, componentIDs[j]);

			// component was present in old archetype and moved into new one during migration:
			// call destructor to ensure clean default construction
			if (oldMask[componentIDs[j]])
			{
				m_componentInfo[componentIDs[j]].m_destructor(componentMem);
			}
			// const_cast is legal assuming that componentData is an array of non-const void * in the move constructor case
			m_componentInfo[componentIDs[j]].m_moveConstructor(componentMem, const_cast<void *>(componentData[j]));
		}
		break;
	}
	default:
		assert(false);
		break;
	}
}

bool ECS::removeComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept
{
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	EntityRecord &entityRecord = m_entityRecords[entity];

	// entity has no components at all
	if (!entityRecord.m_archetype)
	{
		return false;
	}

	// build new component mask
	ComponentMask oldMask = entityRecord.m_archetype->m_componentMask;
	ComponentMask newMask = oldMask;

	for (size_t j = 0; j < componentCount; ++j)
	{
		newMask.set(componentIDs[j], false);
	}

	// entity does not have any of the to be removed components
	if (newMask == oldMask)
	{
		return false;
	}

	if (newMask.none())
	{
		// entity has no more components, so set the archetype to null
		entityRecord = {};
	}
	else
	{
		// find archetype
		Archetype *newArchetype = findOrCreateArchetype(newMask);

		// migrate to new archetype
		entityRecord = newArchetype->migrate(entity, entityRecord);
	}

	return true;
}

Archetype *ECS::findOrCreateArchetype(const ComponentMask &mask) noexcept
{
	assert(mask.any());

	// find archetype
	Archetype *archetype = nullptr;
	for (auto &atPtr : m_archetypes)
	{
		if (atPtr->m_componentMask == mask)
		{
			archetype = atPtr;
			break;
		}
	}

	// create new archetype
	if (!archetype)
	{
		archetype = new Archetype(mask, m_componentInfo);
		m_archetypes.push_back(archetype);
	}

	return archetype;
}

void forEachComponentType(const ComponentMask &mask, const eastl::function<void(size_t index, ComponentID componentID)> &f) noexcept
{
	size_t componentIndex = 0;
	auto componentID = mask.DoFindFirst();
	while (componentID != mask.kSize)
	{
		f(componentIndex, componentID);

		componentID = mask.DoFindNext(componentID);
		++componentIndex;
	}
}
