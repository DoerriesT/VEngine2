#pragma once
#include <EASTL/vector.h>
#include <EASTL/bitset.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <assert.h>
#include "utility/ErasedType.h"

struct Archetype;

constexpr size_t k_ecsMaxComponentTypes = 64;
constexpr size_t k_ecsComponentsPerMemoryChunk = 1024;

using IDType = uint64_t;
using EntityID = IDType;
using ComponentID = IDType;
using ComponentMask = eastl::bitset<k_ecsMaxComponentTypes>;

constexpr EntityID k_nullEntity = 0;

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

void forEachComponentType(const ComponentMask &mask, const eastl::function<void(size_t index, ComponentID componentID)> &f) noexcept;

struct Archetype
{
	ComponentMask m_componentMask = {};
	const ErasedType *m_componentInfo = nullptr;
	size_t m_memoryChunkSize = 0;
	eastl::vector<ArchetypeMemoryChunk> m_memoryChunks;
	eastl::vector<size_t> m_componentArrayOffsets;

	explicit Archetype(const ComponentMask &componentMask, const ErasedType *componentInfo) noexcept;
	size_t getComponentArrayOffset(ComponentID componentID) noexcept;
	ArchetypeSlot allocateDataSlot() noexcept;
	void freeDataSlot(const ArchetypeSlot &slot) noexcept;
	EntityRecord migrate(EntityID entity, const EntityRecord &oldRecord, bool skipConstructorOfMissingComponents = false) noexcept;
	uint8_t *getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) noexcept;
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
	///  Registers a component with the ECS. Must be called on a component type before it can be used in any way.
	/// </summary>
	/// <typeparam name="T">The type of the component to register.</typeparam>
	template<typename T>
	inline void registerComponent() noexcept;

	template<typename T>
	inline void registerSingletonComponent(const T &component) noexcept;

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
	/// Creates a new entity with default constructed components as given by the array of ComponentIDs.
	/// </summary>
	/// <param name="componentCount">The number of components to add to the new entity.</param>
	/// <param name="componentIDs">A pointer to an array of ComponentIDs of the components to add to the new entity.</param>
	/// <returns>The EntityID of the new entity or the null entity if the call failed.</returns>
	EntityID createEntityTypeless(size_t componentCount, const ComponentID *componentIDs) noexcept;

	/// <summary>
	/// Creates a new entity with copy constructed components as given by the array of ComponentIDs.
	/// </summary>
	/// <param name="componentCount">The number of components to add to the new entity.</param>
	/// <param name="componentIDs">A pointer to an array of ComponentIDs of the components to add to the new entity.</param>
	/// <param name="componentData">A pointer to an array of void* where each void* is interpreted as a pointer
	/// to the component corresponding to the ComponentID in the componentIDs array at the same offset.
	/// This data is used to copy construct the new components.</param>
	/// <returns>The EntityID of the new entity or the null entity if the call failed.</returns>
	EntityID createEntityTypeless(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData) noexcept;

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
	/// Adds one or more default constructed components to an entity.
	/// </summary>
	/// <param name="entity">The entity to add the components to.</param>
	/// <param name="componentCount">The number of component types to add.</param>
	/// <param name="componentIDs">A pointer to an array of ComponentIDs to add.</param>
	void addComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept;
	
	/// <summary>
	/// Adds one or more copy constructed components to an entity.
	/// </summary>
	/// <param name="entity">The entity to add the components to.</param>
	/// <param name="componentCount">The number of component types to add.</param>
	/// <param name="componentIDs">A pointer to an array of ComponentIDs to add.</param>
	/// <param name="componentData">A pointer to an array of void* where each void* is interpreted as a pointer
	/// to the component corresponding to the ComponentID in the componentIDs array at the same offset.
	/// This data is used to copy construct the new components.</param>
	void addComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData) noexcept;

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
	/// Removes one or more components from the entity.
	/// </summary>
	/// <param name="entity">The entity to remove the components from.</param>
	/// <param name="componentCount">The number of components to remove.</param>
	/// <param name="componentIDs">A pointer to an array of the ComponentIDs to remove.</param>
	/// <returns>True if any component was removed and false if none of the components is attached to the given entity.</returns>
	bool removeComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept;

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
	/// Gets a void * to a component of an entity.
	/// </summary>
	/// <param name="entity">The entity from which to get the component.</param>
	/// <param name="componentID">The ComponentID of the component to get.</param>
	/// <returns>A pointer to the requested component or nullptr if no such component is attached to the entity.</returns>
	void *getComponentTypeless(EntityID entity, ComponentID componentID) noexcept;

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
	/// Tests if a given entity has a certain set of components.
	/// </summary>
	/// <param name="entity">The entity to test for the components.</param>
	/// <param name="componentCount">The number of component types to check for.</param>
	/// <param name="componentIDs">A pointer to an array of ComponentIDs to check for.</param>
	/// <returns>True if all components are present.</returns>
	bool hasComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept;

	/// <summary>
	/// Gets a mask of all components attached to this entity.
	/// </summary>
	/// <param name="entity">The entity to get the component mask for.</param>
	/// <returns>A mask of all components attached to this entity.</returns>
	ComponentMask getComponentMask(EntityID entity) noexcept;

	ComponentMask getRegisteredComponentMask() noexcept;
	ComponentMask getRegisteredComponentMaskWithSingletons() noexcept;

	/// <summary>
	/// Invokes the given function on all entity/component arrays that contain the requested components.
	/// </summary>
	/// <typeparam name="...T">The components to iterate over.</typeparam>
	/// <param name="func">The function to invoke for each set of matching entity/component arrays.</param>
	template<typename ...T>
	inline void iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func);

	template<typename T>
	inline T *getSingletonComponent() noexcept;

private:
	enum class ComponentConstructorType
	{
		DEFAULT, COPY, MOVE
	};

	EntityID m_nextFreeEntityId = 1;
	eastl::vector<Archetype *> m_archetypes;
	eastl::hash_map<EntityID, EntityRecord> m_entityRecords;
	ErasedType m_componentInfo[k_ecsMaxComponentTypes] = {};
	eastl::bitset<k_ecsMaxComponentTypes> m_singletonComponentsBitset = {};
	void *m_singletonComponents[k_ecsMaxComponentTypes] = {};

	template<typename ...T>
	inline bool isRegisteredComponent() noexcept;
	template<typename ...T>
	inline bool isNotSingletonComponent() noexcept;
	bool isRegisteredComponent(size_t count, const ComponentID *componentIDs) noexcept;
	bool isNotSingletonComponent(size_t count, const ComponentID *componentIDs) noexcept;

	EntityID createEntityInternal(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept;
	void addComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept;
	bool removeComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept;
	Archetype *findOrCreateArchetype(const ComponentMask &mask) noexcept;
};

#include "ECS.inl"