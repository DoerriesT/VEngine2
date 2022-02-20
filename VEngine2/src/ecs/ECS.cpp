#include "ECS.h"
#include <assert.h>
#include "utility/Utility.h"

ComponentID ComponentIDGenerator::m_idCount = 0;

ErasedType ECS::s_componentInfo[k_ecsMaxComponentTypes];
eastl::bitset<k_ecsMaxComponentTypes> ECS::s_singletonComponentsBitset;

ECS::ECS() noexcept
	:m_componentMemoryAllocator(k_componentMemoryChunkSize, 256, "ECS Component Memory Allocator")
{
}

EntityID ECS::createEntity() noexcept
{
	return createEntityInternal(0, nullptr, nullptr, ComponentConstructorType::DEFAULT);
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
	if (entity == k_nullEntity)
	{
		return;
	}

	const uint32_t entityIndex = static_cast<uint32_t>(entity >> 32);
	const uint32_t generation = static_cast<uint32_t>(entity & 0xFFFFFFFF);

	assert(entityIndex < m_entityRecords.size());

	if (m_entityRecords[entityIndex].m_generation == generation && m_entityRecords[entityIndex].m_archetype)
	{
		// delete components
		m_entityRecords[entityIndex].m_archetype->callDestructors(m_entityRecords[entityIndex].m_slot);
		m_entityRecords[entityIndex].m_archetype->freeDataSlot(m_entityRecords[entityIndex].m_slot);

		// delete entity
		freeEntityID(entity);
	}
}

bool ECS::isValid(EntityID entity) const noexcept
{
	const uint32_t entityIndex = static_cast<uint32_t>(entity >> 32);
	const uint32_t generation = static_cast<uint32_t>(entity & 0xFFFFFFFF);

	assert(entity == k_nullEntity || entityIndex < m_entityRecords.size());

	return entity != k_nullEntity && m_entityRecords[entityIndex].m_generation == generation;
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
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	if (entityRecord && entityRecord->m_archetype)
	{
		return entityRecord->m_archetype->getComponentMemory(entityRecord->m_slot, componentID);
	}

	return nullptr;
}

const void *ECS::getComponentTypeless(EntityID entity, ComponentID componentID) const noexcept
{
	assert(isRegisteredComponent(1, &componentID));
	assert(isNotSingletonComponent(1, &componentID));
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	if (entityRecord && entityRecord->m_archetype)
	{
		return entityRecord->m_archetype->getComponentMemory(entityRecord->m_slot, componentID);
	}

	return nullptr;
}

void *ECS::getOrAddComponentTypeless(EntityID entity, ComponentID componentID) noexcept
{
	assert(isRegisteredComponent(1, &componentID));
	assert(isNotSingletonComponent(1, &componentID));
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	if (!entityRecord)
	{
		return nullptr;
	}
	auto *archetype = entityRecord->m_archetype;

	void *component = nullptr;

	// component already exists
	if (archetype && archetype->getComponentMask()[componentID])
	{
		component = archetype->getComponentMemory(entityRecord->m_slot, componentID);
	}
	// add component and migrate to new archetype
	else
	{
		ComponentMask newMask = archetype ? archetype->getComponentMask() : 0;
		newMask.set(componentID, true);

		// find archetype
		Archetype *newArchetype = findOrCreateArchetype(newMask);

		// migrate to new archetype
		*entityRecord = newArchetype->migrate(entity, *entityRecord);
		component = newArchetype->getComponentMemory(entityRecord->m_slot, componentID);
	}

	return component;
}

bool ECS::hasComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) const noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));
	assert(getEntityRecord(entity));

	// simple case
	if (componentCount == 0)
	{
		return true;
	}

	auto *entityRecord = getEntityRecord(entity);
	if (!entityRecord)
	{
		return false;
	}

	// entity has no components at all
	if (componentCount > 0 && !entityRecord->m_archetype)
	{
		return false;
	}

	// check of all requested components are present in the mask
	const auto &archetypeMask = entityRecord->m_archetype->getComponentMask();
	for (size_t j = 0; j < componentCount; ++j)
	{
		if (!archetypeMask[componentIDs[j]])
		{
			return false;
		}
	}

	return true;
}

ComponentMask ECS::getComponentMask(EntityID entity) const noexcept
{
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	return entityRecord && entityRecord->m_archetype ? entityRecord->m_archetype->getComponentMask() : 0;
}

ComponentMask ECS::getRegisteredComponentMask() const noexcept
{
	ComponentMask mask = 0;

	for (size_t i = 0; i < mask.kSize; ++i)
	{
		mask[i] = s_componentInfo[i].m_defaultConstructor != nullptr && !s_singletonComponentsBitset[i];
	}

	return mask;
}

ComponentMask ECS::getRegisteredComponentMaskWithSingletons() const noexcept
{
	ComponentMask mask = 0;

	for (size_t i = 0; i < mask.kSize; ++i)
	{
		mask[i] = s_componentInfo[i].m_defaultConstructor != nullptr;
	}

	return mask;
}

void ECS::clear() noexcept
{
	m_freeEntityIDIndices.clear();
	for (auto archetype : m_archetypes)
	{
		archetype->clear(false);
	}
	m_entityRecords.clear();
	for (auto &sc : m_singletonComponents)
	{
		if (sc)
		{
			delete[] reinterpret_cast<char *>(sc);
			sc = nullptr;
		}
	}
}

bool ECS::isRegisteredComponent(size_t count, const ComponentID *componentIDs) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		if (!s_componentInfo[componentIDs[i]].m_defaultConstructor)
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
		if (s_singletonComponentsBitset[componentIDs[i]])
		{
			return false;
		}
	}
	return true;
}

EntityID ECS::allocateEntityID() noexcept
{
	uint32_t entityIndex;
	uint32_t generation;
	if (m_freeEntityIDIndices.empty())
	{
		entityIndex = static_cast<uint32_t>(m_entityRecords.size());
		generation = 1;

		m_entityRecords.push_back();
	}
	else
	{
		entityIndex = m_freeEntityIDIndices.back();
		m_freeEntityIDIndices.pop_back();
		assert(entityIndex < m_entityRecords.size());
		generation = m_entityRecords[entityIndex].m_generation + 1;
	}

	EntityRecord record{};
	record.m_generation = generation;
	m_entityRecords[entityIndex] = record;

	uint64_t id = 0;
	id |= generation;
	id |= static_cast<uint64_t>(entityIndex) << 32;

	return id;
}

void ECS::freeEntityID(EntityID entity) noexcept
{
	if (entity != k_nullEntity)
	{
		const uint32_t entityIndex = static_cast<uint32_t>(entity >> 32);
		const uint32_t generation = static_cast<uint32_t>(entity & 0xFFFFFFFF);

		assert(entityIndex < m_entityRecords.size());

		if (m_entityRecords[entityIndex].m_generation == generation)
		{
			m_freeEntityIDIndices.push_back(entityIndex);
		}
	}
}

EntityID ECS::createEntityInternal(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept
{
	EntityID entityID = allocateEntityID();

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
			s_componentInfo[componentIDs[j]].m_defaultConstructor(archetype->getComponentMemory(slot, componentIDs[j]));
		}
		break;
	}
	case ECS::ComponentConstructorType::COPY:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			s_componentInfo[componentIDs[j]].m_copyConstructor(archetype->getComponentMemory(slot, componentIDs[j]), componentData[j]);
		}
		break;
	}
	case ECS::ComponentConstructorType::MOVE:
	{
		for (size_t j = 0; j < componentCount; ++j)
		{
			// const_cast is legal assuming that componentData is an array of non-const void * in the move constructor case
			s_componentInfo[componentIDs[j]].m_moveConstructor(archetype->getComponentMemory(slot, componentIDs[j]), const_cast<void *>(componentData[j]));
		}
		break;
	}
	default:
		assert(false);
		break;
	}

	// store entity
	reinterpret_cast<EntityID *>(slot.m_memoryChunk->getMemory())[slot.m_chunkSlotIdx] = entityID;

	EntityRecord record{};
	record.m_archetype = archetype;
	record.m_slot = slot;

	m_entityRecords[entityID >> 32] = record;

	return entityID;
}

void ECS::addComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept
{
	assert(componentCount > 0);
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	if (!entityRecord)
	{
		return;
	}
	auto *archetype = entityRecord->m_archetype;

	// build new component mask
	ComponentMask oldMask = archetype ? archetype->getComponentMask() : 0;
	ComponentMask newMask = oldMask;
	ComponentMask addedComponentsMask = 0;

	for (size_t j = 0; j < componentCount; ++j)
	{
		newMask.set(componentIDs[j], true);
		addedComponentsMask.set(componentIDs[j], true);
	}

	const bool needToMigrate = oldMask != newMask;
	Archetype *newArchetype = needToMigrate ? findOrCreateArchetype(newMask) : archetype;
	if (needToMigrate)
	{
		// pass a mask of all our new components so that their default/move constructor is skipped.
		// this allows us to call our own constructors without having to call a destructor on the memory first.
		*entityRecord = newArchetype->migrate(entity, *entityRecord, &addedComponentsMask);
	}

	for (size_t j = 0; j < componentCount; ++j)
	{
		auto *componentMem = newArchetype->getComponentMemory(entityRecord->m_slot, componentIDs[j]);

		// entity was migrated and we passed a mask of all new components, so the memory is still raw -> we may call our constructors.
		if (needToMigrate)
		{
			switch (constructorType)
			{
			case ECS::ComponentConstructorType::DEFAULT:
				s_componentInfo[componentIDs[j]].m_defaultConstructor(componentMem);
				break;
			case ECS::ComponentConstructorType::COPY:
				s_componentInfo[componentIDs[j]].m_copyConstructor(componentMem, componentData[j]);
				break;
			case ECS::ComponentConstructorType::MOVE:
				// const_cast is legal assuming that componentData is an array of non-const void * in the move constructor case
				s_componentInfo[componentIDs[j]].m_moveConstructor(componentMem, const_cast<void *>(componentData[j]));
				break;
			default:
				assert(false);
				break;
			}
		}
		// special case: the archetype stayed the same, so we simply update the existing components.
		// in the default constructor case, we unfortunately need to call a destructor/constructor pair,
		// but in the copy/move cases we may call the assignment operator instead.
		else
		{
			switch (constructorType)
			{
			case ECS::ComponentConstructorType::DEFAULT:
				// call destructor first for clean default construction
				s_componentInfo[componentIDs[j]].m_destructor(componentMem);
				s_componentInfo[componentIDs[j]].m_defaultConstructor(componentMem);
				break;
			case ECS::ComponentConstructorType::COPY:
				s_componentInfo[componentIDs[j]].m_copyAssign(componentMem, componentData[j]);
				break;
			case ECS::ComponentConstructorType::MOVE:
				// const_cast is legal assuming that componentData is an array of non-const void * in the move assign case
				s_componentInfo[componentIDs[j]].m_moveAssign(componentMem, const_cast<void *>(componentData[j]));
				break;
			default:
				assert(false);
				break;
			}
		}
	}
}

bool ECS::removeComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept
{
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	if (!entityRecord)
	{
		return false;
	}
	auto *archetype = entityRecord->m_archetype;

	// entity has no components at all
	if (!archetype)
	{
		return false;
	}

	// build new component mask
	ComponentMask oldMask = archetype->getComponentMask();
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

	// find archetype
	Archetype *newArchetype = findOrCreateArchetype(newMask);

	// migrate to new archetype
	*entityRecord = newArchetype->migrate(entity, *entityRecord);

	return true;
}

Archetype *ECS::findOrCreateArchetype(const ComponentMask &mask) noexcept
{
	// find archetype
	Archetype *archetype = nullptr;
	for (auto &atPtr : m_archetypes)
	{
		if (atPtr->getComponentMask() == mask)
		{
			archetype = atPtr;
			break;
		}
	}

	// create new archetype
	if (!archetype)
	{
		archetype = new Archetype(this, mask, s_componentInfo);
		m_archetypes.push_back(archetype);
	}

	return archetype;
}

EntityRecord *ECS::getEntityRecord(EntityID entity) noexcept
{
	if (entity != k_nullEntity)
	{
		const uint32_t entityIndex = static_cast<uint32_t>(entity >> 32);
		const uint32_t generation = static_cast<uint32_t>(entity & 0xFFFFFFFF);

		assert(entityIndex < m_entityRecords.size());
		return m_entityRecords[entityIndex].m_generation == generation ? &m_entityRecords[entityIndex] : nullptr;
	}
	return nullptr;
}

const EntityRecord *ECS::getEntityRecord(EntityID entity) const noexcept
{
	if (entity != k_nullEntity)
	{
		const uint32_t entityIndex = static_cast<uint32_t>(entity >> 32);
		const uint32_t generation = static_cast<uint32_t>(entity & 0xFFFFFFFF);

		assert(entityIndex < m_entityRecords.size());
		return m_entityRecords[entityIndex].m_generation == generation ? &m_entityRecords[entityIndex] : nullptr;
	}
	return nullptr;
}

void *ECS::allocateComponentMemoryChunk() noexcept
{
	return m_componentMemoryAllocator.allocate(k_componentMemoryChunkSize);
}

void ECS::freeComponentMemoryChunk(void *ptr) noexcept
{
	m_componentMemoryAllocator.deallocate(ptr, k_componentMemoryChunkSize);
}

