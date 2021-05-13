#include "ECS.h"
#include <assert.h>

Archetype::Archetype(const ComponentMask &componentMask, const ComponentInfo *componentInfo) noexcept
	:m_componentMask(componentMask),
	m_componentInfo(componentInfo),
	m_memoryChunkSize(),
	m_memoryChunks(),
	m_componentArrayOffsets()
{
	constexpr size_t k_baseArrayOffset = sizeof(EntityID) * k_ecsComponentsPerMemoryChunk;

	// compute total chunk size and array offsets by looping over all components of this archetype
	m_memoryChunkSize += k_baseArrayOffset;

	forEachComponentType([this](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];
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

	forEachComponentType([&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];
			uint8_t *freedComp = chunk.m_memory + m_componentArrayOffsets[index] + compInfo.m_size * chunkSlotIdx;
			uint8_t *lastComp = chunk.m_memory + m_componentArrayOffsets[index] + compInfo.m_size * (chunk.m_size - 1);

			// call destructor on to be freed element
			compInfo.destructor(freedComp);

			if (freedComp != lastComp)
			{
				// move last element into free slot
				compInfo.move(freedComp, lastComp);

				// call destructor on last element
				compInfo.destructor(lastComp);
			}
		});

	// swap accompanying entities
	reinterpret_cast<EntityID *>(chunk.m_memory)[chunkSlotIdx] = reinterpret_cast<EntityID *>(chunk.m_memory)[chunk.m_size - 1];

	// reduce chunk size
	--chunk.m_size;
}

EntityRecord Archetype::migrate(EntityID entity, const EntityRecord &oldRecord, bool skipConstructorOfComponent, ComponentID componentToSkipConstructor) noexcept
{
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
	forEachComponentType([&](size_t index, ComponentID componentID)
		{
			if (skipConstructorOfComponent && componentID == componentToSkipConstructor)
			{
				return;
			}

			const auto &compInfo = m_componentInfo[componentID];

			uint8_t *newComp = chunk.m_memory + m_componentArrayOffsets[index] + compInfo.m_size * chunkSlotIdx;

			// old archetype shares this component -> move it
			if (oldRecord.m_archetype && oldRecord.m_archetype->m_componentMask[componentID])
			{
				uint8_t *oldComp = oldChunk->m_memory + oldRecord.m_archetype->getComponentArrayOffset(componentID) + compInfo.m_size * oldChunkSlotIdx;
				compInfo.move(newComp, oldComp);
			}
			// old archetype does not share this component -> default construct it
			else
			{
				compInfo.defaultConstruct(newComp);
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

uint8_t *Archetype::getComponent(const ArchetypeSlot &slot, ComponentID componentID) noexcept
{
	if (!m_componentMask[componentID] || slot.m_chunkIdx >= m_memoryChunks.size() || slot.m_chunkSlotIdx >= m_memoryChunks[slot.m_chunkIdx].m_size)
	{
		return nullptr;
	}

	return m_memoryChunks[slot.m_chunkIdx].m_memory + getComponentArrayOffset(componentID) + m_componentInfo[componentID].m_size * slot.m_chunkSlotIdx;
}

void Archetype::forEachComponentType(const eastl::function<void(size_t index, ComponentID componentID)> &f) noexcept
{
	size_t componentIndex = 0;
	auto componentID = m_componentMask.DoFindFirst();
	while (componentID != m_componentMask.kSize)
	{
		f(componentIndex, componentID);

		componentID = m_componentMask.DoFindNext(componentID);
		++componentIndex;
	}
}

EntityID ECS::createEntity() noexcept
{
	EntityID id = m_nextFreeEntityId++;
	m_entityRecords[id] = {};
	return id;
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
