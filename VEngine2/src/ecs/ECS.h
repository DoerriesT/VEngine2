#pragma once
#include <EASTL/vector.h>
#include <EASTL/bitset.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <assert.h>

struct Archetype;

constexpr size_t k_ecsMaxComponentTypes = 64;
constexpr size_t k_ecsComponentsPerMemoryChunk = 1024;

using IDType = uint64_t;
using EntityID = IDType;
using ComponentID = IDType;
using ComponentMask = eastl::bitset<k_ecsMaxComponentTypes>;

constexpr EntityID k_nullEntity = 0;

template<typename T>
void componentDefaultConstruct(void *mem) noexcept
{
	new (mem) T();
}

template<typename T>
void componentCopyConstruct(void *destination, const void *source) noexcept
{
	new (destination) T(*reinterpret_cast<const T *>(source));
}

template<typename T>
void componentMoveConstruct(void *destination, void *source) noexcept
{
	new (destination) T(eastl::move(*reinterpret_cast<T *>(source)));
}

template<typename T>
void componentDestructor(void *mem) noexcept
{
	reinterpret_cast<T *>(mem)->~T();
}

class ComponentIDGenerator
{
public:
	template<typename T>
	inline static ComponentID getID() noexcept
	{
		static const ComponentID id = m_idCount++;
		assert(id < k_ecsMaxComponentTypes);
		return id;
	}

private:
	static ComponentID m_idCount;
};


struct ComponentInfo
{
	size_t m_size = 0;
	size_t m_alignment = 0;
	void (*m_defaultConstructor)(void *mem) = nullptr;
	void (*m_copyConstructor)(void *destination, const void *source) = nullptr;
	void (*m_moveConstructor)(void *destination, void *source) = nullptr;
	void (*m_destructor)(void *mem) = nullptr;
};

struct ArchetypeSlot
{
	uint32_t m_chunkIdx = 0;
	uint32_t m_chunkSlotIdx = 0;
};

struct EntityRecord
{
	Archetype *m_archetype = nullptr;
	ArchetypeSlot m_slot = {};
};

struct ArchetypeMemoryChunk
{
	uint8_t *m_memory = nullptr;
	size_t m_size = 0;
};

struct Archetype
{
	ComponentMask m_componentMask = {};
	const ComponentInfo *m_componentInfo = nullptr;
	size_t m_memoryChunkSize = 0;
	eastl::vector<ArchetypeMemoryChunk> m_memoryChunks;
	eastl::vector<size_t> m_componentArrayOffsets;

	explicit Archetype(const ComponentMask &componentMask, const ComponentInfo *componentInfo) noexcept;
	size_t getComponentArrayOffset(ComponentID componentID) noexcept;
	ArchetypeSlot allocateDataSlot() noexcept;
	void freeDataSlot(const ArchetypeSlot &slot) noexcept;
	EntityRecord migrate(EntityID entity, const EntityRecord &oldRecord, bool skipConstructorOfMissingComponents = false) noexcept;
	uint8_t *getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) noexcept;
	void forEachComponentType(const eastl::function<void(size_t index, ComponentID componentID)> &f) noexcept;
};

class ECS
{
public:
	template<typename T>
	struct Identity
	{
		using type = T;
	};

	/// <summary>
	/// Registers a component with the ECS. Must be called on a component type before it can be used in any way.
	/// </summary>
	/// <typeparam name="T">The type of the component to register.</typeparam>
	template<typename T>
	inline void registerComponent() noexcept;

	/// <summary>
	/// Creates a new empty entity with no attached components.
	/// </summary>
	/// <returns>The EntityID of the new entity or the null entity if the call failed.</returns>
	EntityID createEntity() noexcept;

	/// <summary>
	/// Creates a new entity with components as given by the template type parameters. The components are default constructed.
	/// This is faster than creating an empty entity and adding components in additional calls.
	/// </summary>
	/// <typeparam name="...T">The types of the components to add to the new entity.</typeparam>
	/// <returns>The EntityID of the new entity or the null entity if the call failed.</returns>
	template<typename ...T>
	inline EntityID createEntity() noexcept;

	/// <summary>
	/// Creates a new entity with components as given by the template type parameters. The components are copy constructed.
	/// This is faster than creating an empty entity and adding components in additional calls.
	/// </summary>
	/// <typeparam name="...T">The types of the components to add to the new entity.</typeparam>
	/// <param name="...components">References to the components to copy into the new entity.</param>
	/// <returns>The EntityID of the new entity or the null entity if the call failed.</returns>
	template<typename ...T>
	inline EntityID createEntity(const T &...components) noexcept;

	/// <summary>
	/// Creates a new entity with components as given by the template type parameters. The components are move constructed.
	/// This is faster than creating an empty entity and adding components in additional calls.
	/// </summary>
	/// <typeparam name="...T">The types of the components to add to the new entity.</typeparam>
	/// <param name="...components">R-value references to the components to move into the new entity.</param>
	/// <returns>The EntityID of the new entity or the null entity if the call failed.</returns>
	template<typename ...T>
	inline EntityID createEntity(T &&...components) noexcept;

	/// <summary>
	/// Destroys the given entity, invalidating the EntityID and removing all attached components.
	/// </summary>
	/// <param name="entity">The entity to destroy. May be the null entity.</param>
	void destroyEntity(EntityID entity) noexcept;

	/// <summary>
	/// Adds a component to an entity, overwriting the previous instance if it was already present.
	/// </summary>
	/// <typeparam name="T">The type of the component to add.</typeparam>
	/// <typeparam name="...Args">The types of the arguments for constructing the new component.</typeparam>
	/// <param name="entity">The entity to add the component to.</param>
	/// <param name="...args">The arguments for constructing the new component.</param>
	/// <returns>A reference to the new component.</returns>
	template<typename T, typename ...Args>
	inline T &addComponent(EntityID entity, Args &&...args) noexcept;

	/// <summary>
	/// Adds one or more default constructed components to an entity.
	/// </summary>
	/// <typeparam name="...T">The types of the components to add.</typeparam>
	/// <param name="entity">The entity to add the components to.</param>
	template<typename ...T>
	inline void addComponents(EntityID entity) noexcept;

	/// <summary>
	/// Adds one or more copy constructed components to an entity.
	/// </summary>
	/// <typeparam name="...T">The types of the components to add.</typeparam>
	/// <param name="entity">The entity to add the components to.</param>
	/// <param name="...components">The components to copy into the entity.</param>
	template<typename ...T>
	inline void addComponents(EntityID entity, const T &...components) noexcept;

	/// <summary>
	/// Adds one or more move constructed components to an entity.
	/// </summary>
	/// <typeparam name="...T">The types of the components to add.</typeparam>
	/// <param name="entity">The entity to add the components to.</param>
	/// <param name="...components">The components to move into the entity.</param>
	template<typename ...T>
	inline void addComponents(EntityID entity, T &&...components) noexcept;

	/// <summary>
	/// Removes a single component from the entity.
	/// </summary>
	/// <typeparam name="T">The type of the component to remove.</typeparam>
	/// <param name="entity">The entity to remove the component from.</param>
	/// <returns>True if the component was removed and false if no component of type T is attached to the given entity.</returns>
	template<typename T>
	inline bool removeComponent(EntityID entity) noexcept;

	/// <summary>
	/// Removes one or more components from the entity.
	/// </summary>
	/// <typeparam name="...T">The types of the components to remove.</typeparam>
	/// <param name="entity">The entity to remove the components from.</param>
	/// <returns>True if any component was removed and false if no component of types ...T is attached to the given entity.</returns>
	template<typename ...T>
	inline bool removeComponents(EntityID entity) noexcept;

	/// <summary>
	/// Adds a component to the entity and removes another.
	/// </summary>
	/// <typeparam name="TAdd">The type of the component to add.</typeparam>
	/// <typeparam name="...Args">The types of the arguments for constructing the new component.</typeparam>
	/// <typeparam name="TRemove">The type of the component to remove.</typeparam>
	/// <param name="entity">The entity to add/remove components to/from.</param>
	/// <param name="...args">The arguments for constructing the new component.</param>
	/// <returns>A reference to the new component.</returns>
	template<typename TAdd, typename TRemove, typename ...Args>
	inline TAdd &addRemoveComponent(EntityID entity, Args &&...args) noexcept;

	/// <summary>
	/// Gets a component from an entity.
	/// </summary>
	/// <typeparam name="T">The type of the component to get.</typeparam>
	/// <param name="entity">The entity from which to get the component.</param>
	/// <returns>A pointer to the requested component or nullptr if no such component is attached to the entity.</returns>
	template<typename T>
	inline T *getComponent(EntityID entity) noexcept;

	/// <summary>
	/// Tests if a given entity has a certain component.
	/// </summary>
	/// <typeparam name="T">The type of the component to check for.</typeparam>
	/// <param name="entity">The entity to test for the component.</param>
	/// <returns>True if the entity has the component.</returns>
	template<typename T>
	inline bool hasComponent(EntityID entity) noexcept;

	/// <summary>
	/// Tests if a given entity has a certain set of components.
	/// </summary>
	/// <typeparam name="...T">The types of the components to check for.</typeparam>
	/// <param name="entity">The entity to test for the components.</param>
	/// <returns>True if the entity has all the given components.</returns>
	template<typename ...T>
	inline bool hasComponents(EntityID entity) noexcept;

	/// <summary>
	/// Invokes the given function on all entity/component arrays that contain the requested components.
	/// </summary>
	/// <typeparam name="...T">The components to iterate over.</typeparam>
	/// <param name="func">The function to invoke for each set of matching entity/component arrays.</param>
	template<typename ...T>
	inline void iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func);

private:
	enum class ComponentConstructorType
	{
		DEFAULT, COPY, MOVE
	};

	EntityID m_nextFreeEntityId = 1;
	eastl::vector<Archetype *> m_archetypes;
	eastl::hash_map<EntityID, EntityRecord> m_entityRecords;
	ComponentInfo m_componentInfo[k_ecsMaxComponentTypes] = {};

	template<typename ...T>
	inline bool isRegisteredComponent() noexcept;

	EntityID createEntityInternal(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept;
	void addComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept;
	Archetype *findOrCreateArchetype(const ComponentMask &mask) noexcept;
};

template<typename T>
inline void ECS::registerComponent() noexcept
{
	ComponentInfo info{};
	info.m_size = sizeof(T);
	info.m_alignment = alignof(T);
	info.m_defaultConstructor = componentDefaultConstruct<T>;
	info.m_copyConstructor = componentCopyConstruct<T>;
	info.m_moveConstructor = componentMoveConstruct<T>;
	info.m_destructor = componentDestructor<T>;

	m_componentInfo[ComponentIDGenerator::getID<T>()] = info;
}

template<typename ...T>
inline EntityID ECS::createEntity() noexcept
{
	assert(isRegisteredComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	return createEntityInternal(sizeof...(T), ids, nullptr, ComponentConstructorType::DEFAULT);
}

template<typename ...T>
inline EntityID ECS::createEntity(const T & ...components) noexcept
{
	assert(isRegisteredComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	const void *srcMemory[sizeof...(T)] = { ((const void *)&components)... };

	return createEntityInternal(sizeof...(T), ids, srcMemory, ComponentConstructorType::COPY);
}

template<typename ...T>
inline EntityID ECS::createEntity(T && ...components) noexcept
{
	assert(isRegisteredComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	void *srcMemory[sizeof...(T)] = { ((void *)&components)... };

	return createEntityInternal(sizeof...(T), ids, srcMemory, ComponentConstructorType::MOVE);
}

template<typename T, typename ...Args>
inline T &ECS::addComponent(EntityID entity, Args &&...args) noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
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

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	addComponentsInternal(entity, sizeof...(T), ids, nullptr, ComponentConstructorType::DEFAULT);
}

template<typename ...T>
inline void ECS::addComponents(EntityID entity, const T & ...components) noexcept
{
	assert(isRegisteredComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	const void *srcMemory[sizeof...(T)] = { ((const void *)&components)... };

	addComponentsInternal(entity, sizeof...(T), ids, srcMemory, ComponentConstructorType::COPY);
}

template<typename ...T>
inline void ECS::addComponents(EntityID entity, T && ...components) noexcept
{
	assert(isRegisteredComponent<T...>());

	ComponentID ids[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };
	void *srcMemory[sizeof...(T)] = { ((void *)&components)... };

	addComponentsInternal(entity, sizeof...(T), ids, srcMemory, ComponentConstructorType::MOVE);
}

template<typename T>
inline bool ECS::removeComponent(EntityID entity) noexcept
{
	const ComponentID componentID = ComponentIDGenerator::getID<T>();

	assert(isRegisteredComponent<T>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	EntityRecord &entityRecord = m_entityRecords[entity];

	// entity does not have this component
	if (!entityRecord.m_archetype || !entityRecord.m_archetype->m_componentMask[componentID])
	{
		return false;
	}
	else
	{
		ComponentMask newMask = entityRecord.m_archetype->m_componentMask;
		newMask.set(componentID, false);
		assert(newMask.count() == (entityRecord.m_archetype->m_componentMask.count() - 1));

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
}

template<typename ...T>
inline bool ECS::removeComponents(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	ComponentID componentIDs[sizeof...(T)] = { (ComponentIDGenerator::getID<T>())... };

	EntityRecord &entityRecord = m_entityRecords[entity];

	// entity has no components at all
	if (!entityRecord.m_archetype)
	{
		return false;
	}

	// build new component mask
	ComponentMask oldMask = entityRecord.m_archetype->m_componentMask;
	ComponentMask newMask = oldMask;

	for (size_t j = 0; j < sizeof...(T); ++j)
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

template<typename TAdd, typename TRemove, typename ...Args>
inline TAdd &ECS::addRemoveComponent(EntityID entity, Args && ...args) noexcept
{
	assert(isRegisteredComponent<TAdd>());
	assert(isRegisteredComponent<TRemove>());
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
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];
	return record.m_archetype && record.m_archetype->m_componentMask[componentID];
}

template<typename ...T>
inline bool ECS::hasComponents(EntityID entity) noexcept
{
	assert(isRegisteredComponent<T...>());
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];
	return record.m_archetype && (... && (record.m_archetype->m_componentMask[ComponentIDGenerator::getID<T>()]));
}

template<typename ...T>
inline void ECS::iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func)
{
	assert(isRegisteredComponent<T...>());

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

template<typename ...T>
inline bool ECS::isRegisteredComponent() noexcept
{
	return (... && (m_componentInfo[ComponentIDGenerator::getID<T>()].m_defaultConstructor != nullptr));
}
