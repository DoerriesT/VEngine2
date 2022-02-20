#pragma once
#include "ECS.h"
#include "utility/Memory.h"

template<typename T>
inline void ECS::registerComponent() noexcept
{
	s_componentInfo[ComponentIDGenerator::getID<T>()] = ErasedType::create<T>();
}

template<typename T>
inline void ECS::registerSingletonComponent() noexcept
{
	s_componentInfo[ComponentIDGenerator::getID<T>()] = ErasedType::create<T>();
	s_singletonComponentsBitset[ComponentIDGenerator::getID<T>()] = true;
}

template<typename ...T>
inline EntityID ECS::createEntity() noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	if constexpr (sizeof...(T) != 0)
	{
		ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
		return createEntityInternal(sizeof...(T), ids, nullptr, ComponentConstructorType::DEFAULT);
	}
	else
	{
		return createEntityInternal(0, nullptr, nullptr, ComponentConstructorType::DEFAULT);
	}
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
inline T *ECS::addComponent(EntityID entity, Args &&...args) noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(getEntityRecord(entity));

	auto *entityRecord = getEntityRecord(entity);
	if (!entityRecord)
	{
		return nullptr;
	}
	auto *archetype = entityRecord->m_archetype;

	T *newComponent = nullptr;

	// component already exists
	if (archetype && archetype->getComponentMask()[componentID])
	{
		newComponent = (T *)archetype->getComponentMemory(entityRecord->m_slot, componentID);

		// move assign
		*newComponent = eastl::move(T(eastl::forward<Args>(args)...));
	}
	// migration to new archetype: skip ctor call in migrate() and manually call ctor afterwards
	else
	{
		ComponentMask newMask = archetype ? archetype->getComponentMask() : 0;
		newMask.set(componentID, true);

		// find archetype
		Archetype *newArchetype = findOrCreateArchetype(newMask);

		ComponentMask newCompMask = 0;
		newCompMask.set(componentID, true);

		// migrate to new archetype and skip constructor of new component
		*entityRecord = newArchetype->migrate(entity, *entityRecord, &newCompMask);
		newComponent = (T *)newArchetype->getComponentMemory(entityRecord->m_slot, componentID);

		// call constructor
		new (newComponent) T(eastl::forward<Args>(args)...);
	}

	return newComponent;
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
inline TAdd *ECS::addRemoveComponent(EntityID entity, Args && ...args) noexcept
{
	assert(isRegisteredComponent<TAdd>());
	assert(isRegisteredComponent<TRemove>());
	assert(isNotSingletonComponent<TAdd>());
	assert(isNotSingletonComponent<TRemove>());
	assert(getEntityRecord(entity));

	const ComponentID addComponentID = ComponentIDGenerator::getID<TAdd>();
	const ComponentID removeComponentID = ComponentIDGenerator::getID<TRemove>();

	auto *entityRecord = getEntityRecord(entity);
	if (!entityRecord)
	{
		return nullptr;
	}
	auto *archetype = entityRecord->m_archetype;

	// build new component mask
	ComponentMask oldMask = archetype ? archetype->getComponentMask() : 0;
	ComponentMask newMask = oldMask;
	newMask.set(removeComponentID, false);
	newMask.set(addComponentID, true);

	TAdd *newComponent = nullptr;

	// archetype didnt change: overwrite old component
	if (oldMask == newMask)
	{
		newComponent = (TAdd *)archetype->getComponentMemory(entityRecord->m_slot, addComponentID);

		// move assign
		*newComponent = eastl::move(TAdd(eastl::forward<Args>(args)...));
	}
	// migration to new archetype: skip ctor call in migrate() and manually call ctor afterwards
	else
	{
		// find archetype
		Archetype *newArchetype = findOrCreateArchetype(newMask);

		ComponentMask newCompMask = 0;
		newCompMask.set(addComponentID, true);

		// migrate to new archetype
		*entityRecord = newArchetype->migrate(entity, *entityRecord, &newCompMask);
		newComponent = (TAdd *)newArchetype->getComponentMemory(entityRecord->m_slot, addComponentID);

		// call constructor
		new (newComponent) TAdd(eastl::forward<Args>(args)...);
	}

	return newComponent;
}

template<typename T>
inline T *ECS::getComponent(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(getEntityRecord(entity));

	const ComponentID componentID = ComponentIDGenerator::getID<T>();
	auto *entityRecord = getEntityRecord(entity);
	if (entityRecord && entityRecord->m_archetype)
	{
		return (T *)entityRecord->m_archetype->getComponentMemory(entityRecord->m_slot, componentID);
	}

	return nullptr;
}

template<typename T>
inline const T *ECS::getComponent(EntityID entity) const noexcept
{
	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(getEntityRecord(entity));

	const ComponentID componentID = ComponentIDGenerator::getID<T>();
	auto *entityRecord = getEntityRecord(entity);
	if (entityRecord && entityRecord->m_archetype)
	{
		return (T *)entityRecord->m_archetype->getComponentMemory(entityRecord->m_slot, componentID);
	}

	return nullptr;
}

template<typename T>
inline T *ECS::getOrAddComponent(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(getEntityRecord(entity));

	const ComponentID componentID = ComponentIDGenerator::getID<T>();
	return (T *)getOrAddComponentTypeless(entity, componentID);
}

template<typename T>
inline bool ECS::hasComponent(EntityID entity) const noexcept
{
	assert(isRegisteredComponent<T>());
	assert(isNotSingletonComponent<T>());
	assert(getEntityRecord(entity));

	const ComponentID componentID = ComponentIDGenerator::getID<T>();
	const auto *record = getEntityRecord(entity);
	return record && record->m_archetype && record->m_archetype->getComponentMask()[componentID];
}

template<typename ...T>
inline bool ECS::hasComponents(EntityID entity) const noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());
	assert(getEntityRecord(entity));

	if (sizeof...(T) == 0)
	{
		return true;
	}

	const auto *record = getEntityRecord(entity);
	return record && record->m_archetype && (... && (record->m_archetype->getComponentMask()[ComponentIDGenerator::getID<T>()]));
}

template<typename ...T>
inline void ECS::setIterateQueryRequiredComponents(IterateQuery &query) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	// build mask
	query.m_requiredComponents = 0;

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	for (size_t j = 0; j < sizeof...(T); ++j)
	{
		query.m_requiredComponents.set(ids[j], true);
	}
}

template<typename ...T>
inline void ECS::setIterateQueryOptionalComponents(IterateQuery &query) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	// build mask
	query.m_optionalComponents = 0;

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	for (size_t j = 0; j < sizeof...(T); ++j)
	{
		query.m_optionalComponents.set(ids[j], true);
	}
}

template<typename ...T>
inline void ECS::setIterateQueryDisallowedComponents(IterateQuery &query) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	// build mask
	query.m_disallowedComponents = 0;

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	for (size_t j = 0; j < sizeof...(T); ++j)
	{
		query.m_disallowedComponents.set(ids[j], true);
	}
}

template<typename ...T, typename F>
inline void ECS::iterate(F &&func) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	// build search mask
	ComponentMask searchMask = 0;

	if constexpr (sizeof...(T) != 0)
	{
		ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

		for (size_t j = 0; j < sizeof...(T); ++j)
		{
			searchMask.set(ids[j], true);
		}
	}

	// search through all archetypes and look for matching masks
	for (auto archetype : m_archetypes)
	{
		// archetype matches search mask
		if ((archetype->getComponentMask() & searchMask) == searchMask)
		{
			auto *chunk = archetype->getMemoryChunkList();
			while (chunk)
			{
				auto chunkSize = chunk->size();
				auto *chunkMem = chunk->getMemory();
				if (chunkSize > 0)
				{
					func(chunkSize, reinterpret_cast<const EntityID *>(chunkMem), (reinterpret_cast<T *>(chunkMem + archetype->getComponentArrayOffset(ComponentIDGenerator::getID<T>())))...);
				}
				chunk = chunk->getNext();
			}
		}
	}
}

template<typename ...T, typename F>
inline void ECS::iterate(const IterateQuery &query, F &&func) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(isNotSingletonComponent<T...>());

	ComponentMask functionSignatureMask = 0;

	if constexpr (sizeof...(T) != 0)
	{
		ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
		for (size_t j = 0; j < sizeof...(T); ++j)
		{
			functionSignatureMask.set(ids[j], true);
		}
	}

	ComponentMask combinedRequiredOptionalMask = query.m_requiredComponents | query.m_optionalComponents;
	assert((combinedRequiredOptionalMask & functionSignatureMask) == functionSignatureMask);

	// search through all archetypes and look for matching masks
	for (auto archetype : m_archetypes)
	{
		const auto &archetypeMask = archetype->getComponentMask();

		if (
			((archetypeMask & query.m_requiredComponents) == query.m_requiredComponents) && // all required components present
			((archetypeMask & query.m_disallowedComponents) == 0) // no disallowed components present
			)
		{
			auto *chunk = archetype->getMemoryChunkList();
			while (chunk)
			{
				auto chunkSize = chunk->size();
				auto *chunkMem = chunk->getMemory();
				if (chunkSize > 0)
				{
					func(chunkSize, reinterpret_cast<const EntityID *>(chunkMem), (reinterpret_cast<T *>(archetypeMask[ComponentIDGenerator::getID<T>()] ? (chunkMem + archetype->getComponentArrayOffset(ComponentIDGenerator::getID<T>())) : nullptr))...);
				}
				chunk = chunk->getNext();
			}
		}
	}
}

template<typename F>
inline void ECS::iterateTypeless(size_t componentCount, const ComponentID *componentIDs, F &&func) noexcept
{
	assert(isRegisteredComponent(componentCount, componentIDs));
	assert(isNotSingletonComponent(componentCount, componentIDs));

	// build search mask
	ComponentMask searchMask = 0;

	for (size_t j = 0; j < componentCount; ++j)
	{
		searchMask.set(componentIDs[j], true);
	}

	void **componentArrayPointers = ALLOC_A_T(void *, componentCount);

	// search through all archetypes and look for matching masks
	for (auto archetype : m_archetypes)
	{
		// archetype matches search mask
		if ((archetype->getComponentMask() & searchMask) == searchMask)
		{
			auto *chunk = archetype->getMemoryChunkList();
			while (chunk)
			{
				auto chunkSize = chunk->size();
				auto *chunkMem = chunk->getMemory();
				if (chunkSize > 0)
				{
					for (size_t j = 0; j < componentCount; ++j)
					{
						componentArrayPointers[j] = chunkMem + archetype->getComponentArrayOffset(componentIDs[j]);
					}
					func(chunkSize, reinterpret_cast<const EntityID *>(chunkMem), componentArrayPointers);
				}
				chunk = chunk->getNext();
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

	if (!m_singletonComponents[componentID])
	{
		const size_t size = s_componentInfo[componentID].m_size;
		void *componentMem = new char[size];
		s_componentInfo[componentID].m_defaultConstructor(componentMem);
		m_singletonComponents[componentID] = componentMem;
	}

	return reinterpret_cast<T *>(m_singletonComponents[componentID]);
}

template<typename T>
inline const T *ECS::getSingletonComponent() const noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(!isNotSingletonComponent<T>());

	if (!m_singletonComponents[componentID])
	{
		const size_t size = s_componentInfo[componentID].m_size;
		void *componentMem = new char[size];
		s_componentInfo[componentID].m_defaultConstructor(componentMem);
		m_singletonComponents[componentID] = componentMem;
	}

	return reinterpret_cast<const T *>(m_singletonComponents[componentID]);
}

template<typename ...T>
inline bool ECS::isRegisteredComponent() noexcept
{
	return (... && (s_componentInfo[ComponentIDGenerator::getID<T>()].m_defaultConstructor != nullptr));
}

template<typename ...T>
inline bool ECS::isNotSingletonComponent() noexcept
{
	return (... && (!s_singletonComponentsBitset[ComponentIDGenerator::getID<T>()]));
}
