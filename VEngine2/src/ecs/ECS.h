#pragma once
#include <EASTL/vector.h>
#include <EASTL/bitset.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>

constexpr size_t k_ecsMaxComponentTypes = 64;
constexpr size_t k_ecsComponentsPerMemoryChunk = 1024;

using IDType = uint64_t;
using EntityID = IDType;
using ComponentID = IDType;
using ArchetypeMask = eastl::bitset<k_ecsMaxComponentTypes>;

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
	void (*defaultConstruct)(void *mem);
	void (*destructor)();
	void (*move)(void *oldMemory, void *newMemory);
};

struct ArchetypeMemoryChunk
{
	uint8_t *m_memory = nullptr;
	size_t m_size = 0;
};

struct Archetype
{
	ArchetypeMask m_componentMask = {};
	eastl::vector<ArchetypeMemoryChunk> m_memoryChunks;
	eastl::vector<size_t> m_componentArrayOffsets;
	Archetype *m_addComponentEdges[k_ecsMaxComponentTypes] = {}; // index with ComponentID
	Archetype *m_removeComponentEdges[k_ecsMaxComponentTypes] = {}; // index with ComponentID

	template<typename T>
	inline size_t getComponentArrayOffset() noexcept
	{
		const auto componentID = getID<T>;
		size_t lastFoundComponentID = componentID;
		size_t numPrevComponents = 0;
		while (lastFoundComponentID != k_ecsMaxComponentTypes)
		{
			lastFoundComponentID = m_componentMask.DoFindPrev(lastFoundComponentID);
			++numPrevComponents;
		}

		return m_componentArrayOffsets[numPrevComponents - 1];
	}

	explicit Archetype(Archetype *parent, ComponentID componentID, size_t additionalTypeSize, size_t additionalTypeAlignment) noexcept;
};

struct EntityRecord
{
	Archetype *m_archetype = nullptr;
	size_t m_index = 0;
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
	void destroyEntity() noexcept;

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

	eastl::vector<Archetype *> m_archetypes;
	eastl::hash_map<EntityID, EntityRecord> m_entityRecords;
	Archetype *m_addComponentRootEdges[k_ecsMaxComponentTypes] = {}; // index with ComponentID
	ComponentInfo m_componentInfo[k_ecsMaxComponentTypes] = {};
};

template<typename T, typename ...Args>
inline T &ECS::addComponent(EntityID entity, Args && ...args) noexcept
{
	EntityRecord &record = m_entityRecords[entity];
	ComponentID componentID = getID<T>;

	auto &addComponentEdgeMap = record.m_archetype ? record.m_archetype->m_addComponentEdges : m_addComponentRootEdges;

	Archetype *newArchetype = addComponentEdgeMap[componentID];

	if (!newArchetype)
	{
		addComponentEdgeMap
	}

	// entity has no components
	if (!record.m_archetype)
	{

	}
}

template<typename ...T>
inline void ECS::iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func)
{
	// build search mask
	ArchetypeMask searchMask;

	IDType ids[] = { (getID<T>())... };
	for (id : ids)
	{
		searchMask.set(id, true);
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
					func(chunk.m_size, reinterpret_cast<const EntityID *>(chunk.m_memory), (reinterpret_cast<T *>(chunk.m_memory + at.getComponentArrayOffset<T>()))...);
				}
			}
		}
	}
}
