#include "Archetype.h"
#include "utility/Utility.h"
#include "ECS.h"

static constexpr size_t k_ecsComponentMemoryChunkSize = 1024 * 16;

Archetype::Archetype(ECS *ecs, const ComponentMask &componentMask, const ErasedType *componentInfo) noexcept
	:m_ecs(ecs),
	m_componentMask(componentMask),
	m_componentInfo(componentInfo)
{
	// compute memory requirements of a single entity
	size_t entityMemoryRequirements = sizeof(EntityID);
	size_t worstCasePaddingRequirements = 0;
	size_t prevAlignment = alignof(EntityID);
	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];
			entityMemoryRequirements += compInfo.m_size;

			// if the alignment of the previous array is a multiple of our alignment, there is no need for padding
			worstCasePaddingRequirements += compInfo.m_alignment > prevAlignment ? compInfo.m_alignment - 1 : 0;
			prevAlignment = compInfo.m_alignment;
		});

	assert(entityMemoryRequirements < k_ecsComponentMemoryChunkSize);
	assert(k_ecsComponentMemoryChunkSize > worstCasePaddingRequirements);
	m_entitiesPerChunk = (k_ecsComponentMemoryChunkSize - worstCasePaddingRequirements) / entityMemoryRequirements;
	assert(m_entitiesPerChunk > 0);

	// compute array offsets by looping over all components of this archetype
	size_t currentOffset = sizeof(EntityID) * m_entitiesPerChunk;

	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];
			currentOffset = util::alignPow2Up<size_t>(currentOffset, compInfo.m_alignment);
			m_componentArrayOffsets[componentID] = currentOffset;
			currentOffset += compInfo.m_size * m_entitiesPerChunk;
		});
	assert(currentOffset <= k_ecsComponentMemoryChunkSize);
}

Archetype::~Archetype() noexcept
{
	// call destructors on all remaining components and free chunk memory
	for (auto &chunk : m_memoryChunks)
	{
		forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
			{
				const auto &compInfo = m_componentInfo[componentID];
				const auto arrayOffset = m_componentArrayOffsets[componentID];

				uint8_t *arr = chunk.m_memory + arrayOffset;

				for (size_t i = 0; i < chunk.m_size; ++i)
				{
					compInfo.m_destructor(arr);
					arr += compInfo.m_size;
				}
			});

		chunk.m_size = 0;
		delete[] chunk.m_memory;
		chunk.m_memory = nullptr;
	}

	m_memoryChunks.clear();
}

const ComponentMask &Archetype::getComponentMask() const noexcept
{
	return m_componentMask;
}

const eastl::vector<ArchetypeMemoryChunk> &Archetype::getMemoryChunks() noexcept
{
	return m_memoryChunks;
}

size_t Archetype::getComponentArrayOffset(ComponentID componentID) const noexcept
{
	assert(m_componentMask[componentID]);
	return m_componentArrayOffsets[componentID];
}

ArchetypeSlot Archetype::allocateDataSlot() noexcept
{
	ArchetypeSlot resultSlot{};

	for (auto &chunk : m_memoryChunks)
	{
		// found an existing chunk with space for our entity
		if (chunk.m_size < m_entitiesPerChunk)
		{
			resultSlot.m_chunkSlotIdx = (uint32_t)chunk.m_size++;
			return resultSlot;
		}
		++resultSlot.m_chunkIdx;
	}

	// no more space in existing memory chunks -> create a new one
	ArchetypeMemoryChunk chunk{};
	chunk.m_memory = new uint8_t[k_ecsComponentMemoryChunkSize];
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

	assert(chunkSlotIdx < chunk.m_size);

	// do swap-and-pop on components and entity
	if (chunk.m_size > 1)
	{
		forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
			{
				const auto &compInfo = m_componentInfo[componentID];
				uint8_t *freedComp = chunk.m_memory + m_componentArrayOffsets[componentID] + compInfo.m_size * chunkSlotIdx;
				uint8_t *lastComp = chunk.m_memory + m_componentArrayOffsets[componentID] + compInfo.m_size * (chunk.m_size - 1);

				if (freedComp != lastComp)
				{
					// move last element into free slot. we may assume that freedComp had its destructor called already.
					compInfo.m_moveConstructor(freedComp, lastComp);

					// call destructor on last element
					compInfo.m_destructor(lastComp);
				}
			});

		// swap accompanying entities
		reinterpret_cast<EntityID *>(chunk.m_memory)[chunkSlotIdx] = reinterpret_cast<EntityID *>(chunk.m_memory)[chunk.m_size - 1];

		// update entity record of last entity that got swapped into the freed slot
		{
			auto *swappedEntityRecord = m_ecs->getEntityRecord(reinterpret_cast<EntityID *>(chunk.m_memory)[chunkSlotIdx]);
			assert(swappedEntityRecord);
			
			swappedEntityRecord->m_slot = slot;
		}
		
	}

	// reduce chunk size
	--chunk.m_size;
}

EntityRecord Archetype::migrate(EntityID entity, const EntityRecord &oldRecord, ComponentMask *constructorsToSkip) noexcept
{
	if (oldRecord.m_archetype == this)
	{
		return oldRecord;
	}

	// allocate a new slot to migrate the entity to
	const auto slot = allocateDataSlot();
	const size_t chunkIdx = slot.m_chunkIdx;
	const size_t chunkSlotIdx = slot.m_chunkSlotIdx;

	auto &chunk = m_memoryChunks[chunkIdx];

	// get chunk and slot in old archetype
	ArchetypeMemoryChunk *oldChunk = nullptr;
	const size_t oldChunkSlotIdx = oldRecord.m_slot.m_chunkSlotIdx;
	if (oldRecord.m_archetype)
	{
		oldChunk = &oldRecord.m_archetype->m_memoryChunks[oldRecord.m_slot.m_chunkIdx];
	}

	// iterate over all components in this archetype and move them
	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			// we should skip the constructor for this component. the calling code
			// is responsible for calling a constructor on the memory after the call.
			if (constructorsToSkip && (*constructorsToSkip)[componentID])
			{
				return;
			}

			const auto &compInfo = m_componentInfo[componentID];

			uint8_t *newComp = chunk.m_memory + m_componentArrayOffsets[componentID] + compInfo.m_size * chunkSlotIdx;

			// old archetype shares this component -> move it
			if (oldRecord.m_archetype && oldRecord.m_archetype->m_componentMask[componentID])
			{
				// get pointer to component in old archetype
				uint8_t *oldComp = oldChunk->m_memory + oldRecord.m_archetype->getComponentArrayOffset(componentID) + compInfo.m_size * oldChunkSlotIdx;
				// move it to its new location
				compInfo.m_moveConstructor(newComp, oldComp);

				// the destructor for the old memory location is called later
			}
			// old archetype does not share this component -> default construct it
			else
			{
				compInfo.m_defaultConstructor(newComp);
			}
		});

	// copy entity
	reinterpret_cast<EntityID *>(chunk.m_memory)[chunkSlotIdx] = entity;

	// call destructors and free slot in old archetype
	if (oldRecord.m_archetype)
	{
		oldRecord.m_archetype->callDestructors(oldRecord.m_slot);
		oldRecord.m_archetype->freeDataSlot(oldRecord.m_slot);
	}

	EntityRecord newRecord{};
	newRecord.m_archetype = this;
	newRecord.m_slot = slot;

	return newRecord;
}

void Archetype::callDestructors(const ArchetypeSlot &slot) noexcept
{
	const auto &chunk = m_memoryChunks[slot.m_chunkIdx];
	const auto slotIdx = slot.m_chunkSlotIdx;

	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_componentInfo[componentID];

			uint8_t *compMem = chunk.m_memory + m_componentArrayOffsets[componentID] + compInfo.m_size * slotIdx;
			compInfo.m_destructor(compMem);
		});
}

uint8_t *Archetype::getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) noexcept
{
	if (!m_componentMask[componentID] || slot.m_chunkIdx >= m_memoryChunks.size() || slot.m_chunkSlotIdx >= m_memoryChunks[slot.m_chunkIdx].m_size)
	{
		return nullptr;
	}

	return m_memoryChunks[slot.m_chunkIdx].m_memory + getComponentArrayOffset(componentID) + m_componentInfo[componentID].m_size * slot.m_chunkSlotIdx;
}

const uint8_t *Archetype::getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) const noexcept
{
	if (!m_componentMask[componentID] || slot.m_chunkIdx >= m_memoryChunks.size() || slot.m_chunkSlotIdx >= m_memoryChunks[slot.m_chunkIdx].m_size)
	{
		return nullptr;
	}

	return m_memoryChunks[slot.m_chunkIdx].m_memory + getComponentArrayOffset(componentID) + m_componentInfo[componentID].m_size * slot.m_chunkSlotIdx;
}

void Archetype::clear(bool clearReferenceInECS) noexcept
{
	// call all destructors and reset the size of each memory chunk
	const size_t numChunks = m_memoryChunks.size();
	for (uint32_t chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx)
	{
		auto &chunk = m_memoryChunks[chunkIdx];

		// call destructors on all components
		if (chunk.m_size > 0)
		{
			forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
				{
					const auto &compInfo = m_componentInfo[componentID];

					for (uint32_t i = 0; i < chunk.m_size; ++i)
					{
						uint8_t *compMem = chunk.m_memory + m_componentArrayOffsets[componentID] + compInfo.m_size * i;
						compInfo.m_destructor(compMem);
					}
				});

			if (clearReferenceInECS)
			{
				// clear EntityRecords in ECS
				for (size_t i = 0; i < chunk.m_size; ++i)
				{
					auto *record = m_ecs->getEntityRecord(reinterpret_cast<EntityID *>(chunk.m_memory)[i]);
					assert(record);
					record->m_archetype = nullptr;
					record->m_slot = {};
				}
			}

			chunk.m_size = 0;
		}
	}
}
