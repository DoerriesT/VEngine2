#include "Archetype.h"
#include "utility/Utility.h"
#include "ECS.h"

Archetype::Archetype(ECS *ecs, const ComponentMask &componentMask, const ErasedType *componentInfo) noexcept
	:m_ecs(ecs),
	m_componentMask(componentMask)
{
	// compute memory requirements of a single entity
	size_t entityMemoryRequirements = sizeof(EntityID);
	size_t worstCasePaddingRequirements = 0;
	size_t prevAlignment = alignof(EntityID);
	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_ecs->s_componentInfo[componentID];
			entityMemoryRequirements += compInfo.m_size;

			// if the alignment of the previous array is a multiple of our alignment, there is no need for padding
			worstCasePaddingRequirements += compInfo.m_alignment > prevAlignment ? compInfo.m_alignment - 1 : 0;
			prevAlignment = compInfo.m_alignment;
		});

	assert(entityMemoryRequirements < ECS::k_componentMemoryChunkSize);
	assert(ECS::k_componentMemoryChunkSize > worstCasePaddingRequirements);
	m_entitiesPerChunk = (ECS::k_componentMemoryChunkSize - worstCasePaddingRequirements - ArchetypeMemoryChunk::getDataOffset()) / entityMemoryRequirements;
	assert(m_entitiesPerChunk > 0);

	// compute array offsets by looping over all components of this archetype
	size_t currentOffset = sizeof(EntityID) * m_entitiesPerChunk;

	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_ecs->s_componentInfo[componentID];
			currentOffset = util::alignPow2Up<size_t>(currentOffset, compInfo.m_alignment);
			m_componentArrayOffsets[componentID] = currentOffset;
			currentOffset += compInfo.m_size * m_entitiesPerChunk;
		});
	assert(currentOffset <= ECS::k_componentMemoryChunkSize);
}

Archetype::Archetype(Archetype &&other) noexcept
	:m_ecs(other.m_ecs),
	m_memoryChunkList(other.m_memoryChunkList),
	m_componentMask(other.m_componentMask),
	m_entitiesPerChunk(other.m_entitiesPerChunk)
{
	memcpy(m_componentArrayOffsets, other.m_componentArrayOffsets, sizeof(m_componentArrayOffsets));
	other.m_memoryChunkList = nullptr;
}

Archetype &Archetype::operator=(Archetype &&other) noexcept
{
	if (&other != this)
	{
		clear(true);

		m_ecs = other.m_ecs;
		m_memoryChunkList = other.m_memoryChunkList;
		m_componentMask = other.m_componentMask;
		m_entitiesPerChunk = other.m_entitiesPerChunk;
		memcpy(m_componentArrayOffsets, other.m_componentArrayOffsets, sizeof(m_componentArrayOffsets));
		other.m_memoryChunkList = nullptr;
	}

	return *this;
}

Archetype::~Archetype() noexcept
{
	clear(true);
}

const ComponentMask &Archetype::getComponentMask() const noexcept
{
	return m_componentMask;
}

ArchetypeMemoryChunk *Archetype::getMemoryChunkList() noexcept
{
	return m_memoryChunkList;
}

size_t Archetype::getComponentArrayOffset(ComponentID componentID) const noexcept
{
	assert(m_componentMask[componentID]);
	return m_componentArrayOffsets[componentID];
}

ArchetypeSlot Archetype::allocateDataSlot() noexcept
{
	auto *chunk = m_memoryChunkList;
	while (chunk)
	{
		// found an existing chunk with space for our entity
		if (chunk->m_size < m_entitiesPerChunk)
		{
			ArchetypeSlot resultSlot{};
			resultSlot.m_memoryChunk = chunk;
			resultSlot.m_chunkSlotIdx = static_cast<uint32_t>(chunk->m_size++);
			return resultSlot;
		}
		chunk = chunk->m_next;
	}

	// no more space in existing memory chunks -> create a new one
	chunk = reinterpret_cast<ArchetypeMemoryChunk *>(m_ecs->allocateComponentMemoryChunk());
	*chunk = {};
	chunk->m_size = 1;
	chunk->m_prev = nullptr;
	chunk->m_next = m_memoryChunkList; // new chunk now points to old list head
	m_memoryChunkList = chunk;

	if (chunk->m_next)
	{
		assert(!chunk->m_next->m_prev); // assert that the old list head has m_prev set to null
		chunk->m_next->m_prev = chunk;
	}

	ArchetypeSlot resultSlot{};
	resultSlot.m_memoryChunk = chunk;
	resultSlot.m_chunkSlotIdx = 0;

	return resultSlot;
}

void Archetype::freeDataSlot(const ArchetypeSlot &slot) noexcept
{
	auto *chunk = slot.m_memoryChunk;
	const size_t chunkSlotIdx = slot.m_chunkSlotIdx;

	assert(chunk);
	assert(chunkSlotIdx < chunk->m_size);

	// do swap-and-pop on components and entity
	if (chunk->m_size > 1)
	{
		auto *chunkMem = chunk->getMemory();
		forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
			{
				const auto &compInfo = m_ecs->s_componentInfo[componentID];
				uint8_t *freedComp = chunkMem + m_componentArrayOffsets[componentID] + compInfo.m_size * chunkSlotIdx;
				uint8_t *lastComp = chunkMem + m_componentArrayOffsets[componentID] + compInfo.m_size * (chunk->m_size - 1);

				if (freedComp != lastComp)
				{
					// move last element into free slot. we may assume that freedComp had its destructor called already.
					compInfo.m_moveConstructor(freedComp, lastComp);

					// call destructor on last element
					compInfo.m_destructor(lastComp);
				}
			});

		EntityID *entityArray = reinterpret_cast<EntityID *>(chunkMem);
		// swap accompanying entities
		entityArray[chunkSlotIdx] = entityArray[chunk->m_size - 1];

		// update entity record of last entity that got swapped into the freed slot
		{
			auto *swappedEntityRecord = m_ecs->getEntityRecord(entityArray[chunkSlotIdx]);
			assert(swappedEntityRecord);

			swappedEntityRecord->m_slot = slot;
		}

	}

	// reduce chunk size
	--chunk->m_size;

	// free chunk
	if (chunk->m_size == 0)
	{
		if (chunk->m_next)
		{
			chunk->m_next->m_prev = chunk->m_prev;
		}
		if (chunk->m_prev)
		{
			chunk->m_prev->m_next = chunk->m_next;
		}
		if (m_memoryChunkList == chunk)
		{
			assert(!chunk->m_prev);
			m_memoryChunkList = chunk->m_next;
		}
		m_ecs->freeComponentMemoryChunk(chunk);
	}
}

EntityRecord Archetype::migrate(EntityID entity, const EntityRecord &oldRecord, ComponentMask *constructorsToSkip) noexcept
{
	if (oldRecord.m_archetype == this)
	{
		return oldRecord;
	}

	// allocate a new slot to migrate the entity to
	const auto slot = allocateDataSlot();
	auto *chunk = slot.m_memoryChunk;
	const size_t chunkSlotIdx = slot.m_chunkSlotIdx;

	// get chunk and slot in old archetype
	ArchetypeMemoryChunk *oldChunk = nullptr;
	const size_t oldChunkSlotIdx = oldRecord.m_slot.m_chunkSlotIdx;
	if (oldRecord.m_archetype)
	{
		oldChunk = oldRecord.m_slot.m_memoryChunk;
	}

	auto *chunkMem = chunk->getMemory();
	auto *oldChunkMem = oldChunk ? oldChunk->getMemory() : nullptr;

	// iterate over all components in this archetype and move them
	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			// we should skip the constructor for this component. the calling code
			// is responsible for calling a constructor on the memory after the call.
			if (constructorsToSkip && (*constructorsToSkip)[componentID])
			{
				return;
			}

			const auto &compInfo = m_ecs->s_componentInfo[componentID];

			uint8_t *newComp = chunkMem + m_componentArrayOffsets[componentID] + compInfo.m_size * chunkSlotIdx;

			// old archetype shares this component -> move it
			if (oldRecord.m_archetype && oldRecord.m_archetype->m_componentMask[componentID])
			{
				// get pointer to component in old archetype
				uint8_t *oldComp = oldChunkMem + oldRecord.m_archetype->m_componentArrayOffsets[componentID] + compInfo.m_size * oldChunkSlotIdx;
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
	reinterpret_cast<EntityID *>(chunkMem)[chunkSlotIdx] = entity;

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
	auto *chunk = slot.m_memoryChunk;
	auto *chunkMem = chunk->getMemory();
	const auto slotIdx = slot.m_chunkSlotIdx;

	forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
		{
			const auto &compInfo = m_ecs->s_componentInfo[componentID];

			uint8_t *compMem = chunkMem + m_componentArrayOffsets[componentID] + compInfo.m_size * slotIdx;
			compInfo.m_destructor(compMem);
		});
}

uint8_t *Archetype::getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) noexcept
{
	if (!m_componentMask[componentID] || !slot.m_memoryChunk || slot.m_chunkSlotIdx >= slot.m_memoryChunk->m_size)
	{
		return nullptr;
	}

	return slot.m_memoryChunk->getMemory() + m_componentArrayOffsets[componentID] + m_ecs->s_componentInfo[componentID].m_size * slot.m_chunkSlotIdx;
}

const uint8_t *Archetype::getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) const noexcept
{
	if (!m_componentMask[componentID] || !slot.m_memoryChunk || slot.m_chunkSlotIdx >= slot.m_memoryChunk->m_size)
	{
		return nullptr;
	}

	return slot.m_memoryChunk->getMemory() + m_componentArrayOffsets[componentID] + m_ecs->s_componentInfo[componentID].m_size * slot.m_chunkSlotIdx;
}

void Archetype::clear(bool clearReferenceInECS) noexcept
{
	// call destructors on all remaining components and free chunk memory
	auto *chunk = m_memoryChunkList;
	while (chunk)
	{
		if (chunk->m_size > 0)
		{
			forEachComponentType(m_componentMask, [&](size_t index, ComponentID componentID)
				{
					const auto &compInfo = m_ecs->s_componentInfo[componentID];
					uint8_t *arr = chunk->getMemory() + m_componentArrayOffsets[componentID];

					for (size_t i = 0; i < chunk->m_size; ++i)
					{
						compInfo.m_destructor(arr);
						arr += compInfo.m_size;
					}
				});

			if (clearReferenceInECS)
			{
				// clear EntityRecords in ECS
				for (size_t i = 0; i < chunk->m_size; ++i)
				{
					auto *record = m_ecs->getEntityRecord(reinterpret_cast<EntityID *>(chunk->getMemory())[i]);
					assert(record);
					record->m_archetype = nullptr;
					record->m_slot = {};
				}
			}
		}


		auto *ptr = chunk;
		chunk = chunk->m_next;
		m_ecs->freeComponentMemoryChunk(ptr);
	}

	m_memoryChunkList = nullptr;
}
