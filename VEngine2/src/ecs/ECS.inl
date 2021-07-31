#pragma once
#include "ECS.h"

template<typename T>
inline void ECS::registerComponent() noexcept
{
	m_componentInfo[ComponentIDGenerator::getID<T>()] = ErasedType::create<T>();
}

template<typename T>
inline void ECS::registerSingletonComponent(const T &component) noexcept
{
	m_componentInfo[ComponentIDGenerator::getID<T>()] = ErasedType::create<T>();
	m_singletonComponents[ComponentIDGenerator::getID<T>()] = new T{ component };
	m_singletonComponentsBitset[ComponentIDGenerator::getID<T>()] = true;
}

template<typename ...T>
inline EntityID ECS::createEntity() noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	return createEntityInternal(sizeof...(T), ids, nullptr, ComponentConstructorType::DEFAULT);
}

template<typename ...T>
inline EntityID ECS::createEntity(const T & ...components) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	const void *srcMemory[sizeof...(T)] = { ((const void *)&components)... };

	return createEntityInternal(sizeof...(T), ids, srcMemory, ComponentConstructorType::COPY);
}

template<typename ...T>
inline EntityID ECS::createEntity(T && ...components) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	void *srcMemory[sizeof...(T)] = { ((void *)&components)... };

	return createEntityInternal(sizeof...(T), ids, srcMemory, ComponentConstructorType::MOVE);
}

template<typename T, typename ...Args>
inline T &ECS::addComponent(EntityID entity, Args &&...args) noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	EntityRecord &entityRecord = m_entityRecords[entity];

	T *newComponent = nullptr;

	// component already exists
	if (entityRecord.m_archetype && entityRecord.m_archetype->m_componentMask[componentID])
	{
		newComponent = (T *)entityRecord.m_archetype->getComponentMemory(entityRecord.m_slot, componentID);
	}
	else
	{
		ComponentMask newMask = entityRecord.m_archetype ? entityRecord.m_archetype->m_componentMask : 0;
		newMask.set(componentID, true);

		// find archetype
		Archetype *newArchetype = findOrCreateArchetype(newMask);

		// migrate to new archetype
		entityRecord = newArchetype->migrate(entity, entityRecord, true);
		newComponent = (T *)newArchetype->getComponentMemory(entityRecord.m_slot, componentID);
	}

	new (newComponent) T(eastl::forward<Args>(args)...);
	return *newComponent;
}

template<typename ...T>
inline void ECS::addComponents(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	addComponentsInternal(entity, sizeof...(T), ids, nullptr, ComponentConstructorType::DEFAULT);
}

template<typename ...T>
inline void ECS::addComponents(EntityID entity, const T & ...components) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	const void *srcMemory[sizeof...(T)] = { ((const void *)&components)... };

	addComponentsInternal(entity, sizeof...(T), ids, srcMemory, ComponentConstructorType::COPY);
}

template<typename ...T>
inline void ECS::addComponents(EntityID entity, T && ...components) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	void *srcMemory[sizeof...(T)] = { ((void *)&components)... };

	addComponentsInternal(entity, sizeof...(T), ids, srcMemory, ComponentConstructorType::MOVE);
}

template<typename T>
inline bool ECS::removeComponent(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	const ComponentID componentID = ComponentIDGenerator::getID<T>();
	return removeComponentsInternal(entity, 1, &componentID);
}

template<typename ...T>
inline bool ECS::removeComponents(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());
	ComponentID componentIDs[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	return removeComponentsInternal(entity, sizeof...(T), componentIDs);
}

template<typename TAdd, typename TRemove, typename ...Args>
inline TAdd &ECS::addRemoveComponent(EntityID entity, Args && ...args) noexcept
{
	assert(isRegisteredComponent<TAdd>());
	assert(isRegisteredComponent<TRemove>());
	assert(isNotSingletonComponent<TAdd>());
	assert(isNotSingletonComponent<TRemove>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	const ComponentID addComponentID = ComponentIDGenerator::getID<TAdd>();
	const ComponentID removeComponentID = ComponentIDGenerator::getID<TRemove>();

	EntityRecord &entityRecord = m_entityRecords[entity];

	// build new component mask
	ComponentMask oldMask = entityRecord.m_archetype ? entityRecord.m_archetype->m_componentMask : 0;
	ComponentMask newMask = oldMask;
	newMask.set(removeComponentID, false);
	newMask.set(addComponentID, true);

	TAdd *newComponent = nullptr;

	// archetype didnt change: overwrite old component
	if (oldMask == newMask)
	{
		newComponent = (TAdd *)entityRecord.m_archetype->getComponentMemory(entityRecord.m_slot, addComponentID);
		m_componentInfo[addComponentID].m_destructor(newComponent);
	}
	else
	{
		// find archetype
		Archetype *newArchetype = findOrCreateArchetype(newMask);

		// migrate to new archetype
		entityRecord = newArchetype->migrate(entity, entityRecord, true);
		newComponent = (TAdd *)newArchetype->getComponentMemory(entityRecord.m_slot, addComponentID);
	}

	new (newComponent) TAdd(eastl::forward<Args>(args)...);

	return *newComponent;
}

template<typename T>
inline T *ECS::getComponent(EntityID entity) noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];

	if (record.m_archetype)
	{
		return (T *)record.m_archetype->getComponentMemory(record.m_slot, componentID);
	}

	return nullptr;
}

template<typename T>
inline bool ECS::hasComponent(EntityID entity) noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];
	return record.m_archetype && record.m_archetype->m_componentMask[componentID];
}

template<typename ...T>
inline bool ECS::hasComponents(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	if (sizeof...(T) == 0)
	{
		return true;
	}

	auto record = m_entityRecords[entity];
	return record.m_archetype && (... && (record.m_archetype->m_componentMask[ComponentIDGenerator::getID<T>()]));
}

template<typename ...T>
inline void ECS::iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func)
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	// build search mask
	ComponentMask searchMask = 0;

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	for (size_t j = 0; j < sizeof...(T); ++j)
	{
		searchMask.set(ids[j], true);
	}

	// search through all archetypes and look for matching masks
	for (auto &atPtr : m_archetypes)
	{
		auto &at = *atPtr;
		// archetype matches search mask
		if ((at.m_componentMask & searchMask) == searchMask)
		{
			for (auto &chunk : at.m_memoryChunks)
			{
				if (chunk.m_size > 0)
				{
					func(chunk.m_size, reinterpret_cast<const EntityID *>(chunk.m_memory), (reinterpret_cast<T *>(chunk.m_memory + at.getComponentArrayOffset(ComponentIDGenerator::getID<T>())))...);
				}
			}
		}
	}
}

template<typename T>
inline T *ECS::getSingletonComponent() noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(!isNotSingletonComponent<T>());

	return reinterpret_cast<T *>(m_singletonComponents[componentID]);
}

template<typename ...T>
inline bool ECS::isRegisteredComponent() noexcept
{
	return (... && (m_componentInfo[ComponentIDGenerator::getID<T>()].m_defaultConstructor != nullptr));
}

template<typename ...T>
inline bool ECS::isNotSingletonComponent() noexcept
{
	return (... && (!m_singletonComponentsBitset[ComponentIDGenerator::getID<T>()]));
}