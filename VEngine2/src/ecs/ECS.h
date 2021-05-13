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

template<typename T>
void componentDefaultConstruct(void *mem) noexcept
{
	new (mem) T();
}

template<typename T>
void componentDestructor(void *mem) noexcept
{
	reinterpret_cast<T *>(mem)->~T();
}

template<typename T>
void componentMove(void *destination, void *source) noexcept
{
	new (destination) T(eastl::move(*reinterpret_cast<T *>(source)));
}

template<typename T>
void componentAssign(void *destination, void *source) noexcept
{
	*reinterpret_cast<T *>(destination) = *reinterpret_cast<const T *>(source);
}

static IDType m_idCount;

template<typename T>
static IDType getID() noexcept
{
	static const IDType id = m_idCount++;
	return id;
}

struct ComponentInfo
{
	size_t m_size = 0;
	size_t m_alignment = 0;
	void (*defaultConstruct)(void *mem) = nullptr;
	void (*destructor)(void *mem) = nullptr;
	void (*move)(void *destination, void *source) = nullptr;
	void (*assign)(void *destination, void *source) = nullptr;
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
	EntityRecord migrate(EntityID entity, const EntityRecord &oldRecord, bool skipConstructorOfComponent = false, ComponentID componentToSkipConstructor = 0) noexcept;
	uint8_t *getComponent(const ArchetypeSlot &slot, ComponentID componentID) noexcept;
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

	EntityID createEntity() noexcept;
	void destroyEntity(EntityID entity) noexcept;

	template<typename T>
	void registerComponent() noexcept;

	template<typename T, typename ...Args>
	inline T &addComponent(EntityID entity, Args &&...args) noexcept;

	template<typename T>
	inline bool removeComponent(EntityID entity) noexcept;

	template<typename T>
	inline T *getComponent(EntityID entity) noexcept;

	template<typename T>
	inline bool hasComponent(EntityID entity) noexcept;

	template<typename ...T>
	inline void iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func);

private:
	EntityID m_nextFreeEntityId = 1;
	eastl::vector<Archetype *> m_archetypes;
	eastl::hash_map<EntityID, EntityRecord> m_entityRecords;
	ComponentInfo m_componentInfo[k_ecsMaxComponentTypes] = {};
};

template<typename T>
inline void ECS::registerComponent() noexcept
{
	ComponentInfo info{};
	info.m_size = sizeof(T);
	info.m_alignment = alignof(T);
	info.defaultConstruct = componentDefaultConstruct<T>;
	info.destructor = componentDestructor<T>;
	info.move = componentMove<T>;
	info.assign = componentAssign<T>;

	m_componentInfo[getID<T>()] = info;
}

template<typename T, typename ...Args>
inline T &ECS::addComponent(EntityID entity, Args &&...args) noexcept
{
	const ComponentID componentID = getID<T>();

	// assert that the component was registered
	assert(m_componentInfo[componentID].defaultConstruct);
	assert(m_entityRecords.find(entity) != m_entityRecords.end());
	
	EntityRecord &entityRecord = m_entityRecords[entity];

	T *newComponent = nullptr;

	// component already exists
	if (entityRecord.m_archetype && entityRecord.m_archetype->m_componentMask[componentID])
	{
		newComponent = (T *)entityRecord.m_archetype->getComponent(entityRecord.m_slot, componentID);
	}
	else
	{
		ComponentMask newMask = entityRecord.m_archetype ? entityRecord.m_archetype->m_componentMask : 0;
		newMask.set(componentID, true);

		// find archetype
		Archetype *newArchetype = nullptr;
		for (auto &atPtr : m_archetypes)
		{
			if (atPtr->m_componentMask == newMask)
			{
				newArchetype = atPtr;
				break;
			}
		}

		// create new archetype
		if (!newArchetype)
		{
			newArchetype = new Archetype(newMask, m_componentInfo);
			m_archetypes.push_back(newArchetype);
		}

		// migrate to new archetype
		entityRecord = newArchetype->migrate(entity, entityRecord, true, componentID);
		newComponent = (T*)newArchetype->getComponent(entityRecord.m_slot, componentID);
	}

	*newComponent = T(eastl::forward<Args>(args)...);
	return *newComponent;
}

template<typename T>
inline bool ECS::removeComponent(EntityID entity) noexcept
{
	const ComponentID componentID = getID<T>();

	// assert that the component was registered
	assert(m_componentInfo[componentID].defaultConstruct);
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	EntityRecord &entityRecord = m_entityRecords[entity];

	T *newComponent = nullptr;

	// entity does not have this component
	if (!entityRecord.m_archetype || !entityRecord.m_archetype->m_componentMask[componentID])
	{
		return false;
	}
	else
	{
		ComponentMask newMask = entityRecord.m_archetype->m_componentMask;
		newMask.set(componentID, false);

		// find archetype
		Archetype *newArchetype = nullptr;
		for (auto &atPtr : m_archetypes)
		{
			if (atPtr->m_componentMask == newMask)
			{
				newArchetype = atPtr;
				break;
			}
		}

		// create new archetype
		if (!newArchetype)
		{
			newArchetype = new Archetype(newMask, m_componentInfo);
			m_archetypes.push_back(newArchetype);
		}

		// migrate to new archetype
		entityRecord = newArchetype->migrate(entity, entityRecord);

		return true;
	}
}

template<typename T>
inline T *ECS::getComponent(EntityID entity) noexcept
{
	const ComponentID componentID = getID<T>();

	// assert that the component was registered
	assert(m_componentInfo[componentID].defaultConstruct);
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];

	if (record.m_archetype)
	{
		return (T *)record.m_archetype->getComponent(record.m_slot, componentID);
	}

	return nullptr;
}

template<typename T>
inline bool ECS::hasComponent(EntityID entity) noexcept
{
	const ComponentID componentID = getID<T>();

	// assert that the component was registered
	assert(m_componentInfo[componentID].defaultConstruct);
	assert(m_entityRecords.find(entity) != m_entityRecords.end());

	auto record = m_entityRecords[entity];
	return record.m_archetype && record.m_archetype->m_componentMask[componentID];
}

template<typename ...T>
inline void ECS::iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func)
{
	// build search mask
	ComponentMask searchMask = 0;

	IDType ids[sizeof...(T)] = { (getID<T>())... };
	
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
					func(chunk.m_size, reinterpret_cast<const EntityID *>(chunk.m_memory), (reinterpret_cast<T *>(chunk.m_memory + at.getComponentArrayOffset(getID<T>())))...);
				}
			}
		}
	}
}
