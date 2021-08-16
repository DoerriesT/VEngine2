#pragma once
#include <EASTL/vector.h>
#include <EASTL/bitset.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <assert.h>
#include "utility/ErasedType.h"
#include "ECSCommon.h"
#include "Archetype.h"

class Archetype;

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

	/// <summary>
	/// Registers a singleton component. This call allocates the component and copy-constructs it with the given argument.
	/// After this call, it can be accessed with getSingletonComponent().
	/// </summary>
	/// <typeparam name="T">The type of the singleton component to register.</typeparam>
	/// <param name="component">An instance of the singleton component to copy-construct the ECS internal instance.</param>
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
	/// Gets a component from an entity.
	/// </summary>
	/// <typeparam name="T">The type of the component to get.</typeparam>
	/// <param name="entity">The entity from which to get the component.</param>
	/// <returns>A pointer to the requested component or nullptr if no such component is attached to the entity.</returns>
	template<typename T>
	inline const T *getComponent(EntityID entity) const noexcept;

	/// <summary>
	/// Gets a void * to a component of an entity.
	/// </summary>
	/// <param name="entity">The entity from which to get the component.</param>
	/// <param name="componentID">The ComponentID of the component to get.</param>
	/// <returns>A pointer to the requested component or nullptr if no such component is attached to the entity.</returns>
	void *getComponentTypeless(EntityID entity, ComponentID componentID) noexcept;

	/// <summary>
	/// Gets a void * to a component of an entity.
	/// </summary>
	/// <param name="entity">The entity from which to get the component.</param>
	/// <param name="componentID">The ComponentID of the component to get.</param>
	/// <returns>A pointer to the requested component or nullptr if no such component is attached to the entity.</returns>
	const void *getComponentTypeless(EntityID entity, ComponentID componentID) const noexcept;

	/// <summary>
	/// Tests if a given entity has a certain component.
	/// </summary>
	/// <typeparam name="T">The type of the component to check for.</typeparam>
	/// <param name="entity">The entity to test for the component.</param>
	/// <returns>True if the entity has the component.</returns>
	template<typename T>
	inline bool hasComponent(EntityID entity) const noexcept;

	/// <summary>
	/// Tests if a given entity has a certain set of components.
	/// </summary>
	/// <typeparam name="...T">The types of the components to check for.</typeparam>
	/// <param name="entity">The entity to test for the components.</param>
	/// <returns>True if the entity has all the given components.</returns>
	template<typename ...T>
	inline bool hasComponents(EntityID entity) const noexcept;

	/// <summary>
	/// Tests if a given entity has a certain set of components.
	/// </summary>
	/// <param name="entity">The entity to test for the components.</param>
	/// <param name="componentCount">The number of component types to check for.</param>
	/// <param name="componentIDs">A pointer to an array of ComponentIDs to check for.</param>
	/// <returns>True if all components are present.</returns>
	bool hasComponentsTypeless(EntityID entity, size_t componentCount, const ComponentID *componentIDs) const noexcept;

	/// <summary>
	/// Gets a mask of all components attached to this entity.
	/// </summary>
	/// <param name="entity">The entity to get the component mask for.</param>
	/// <returns>A mask of all components attached to this entity.</returns>
	ComponentMask getComponentMask(EntityID entity) const noexcept;

	/// <summary>
	/// Gets the mask of all registered components. This does not include singleton components. See getRegisteredComponentMaskWithSingletons().
	/// </summary>
	/// <returns>The mask of registered components.</returns>
	ComponentMask getRegisteredComponentMask() const noexcept;

	/// <summary>
	/// Gets a mask of all registered components including singleton components.
	/// </summary>
	/// <returns>The mask of registered components including singleton components.</returns>
	ComponentMask getRegisteredComponentMaskWithSingletons() const noexcept;

	/// <summary>
	/// Invokes the given function on all entity/component arrays that contain the requested components.
	/// </summary>
	/// <typeparam name="...T">The components to iterate over.</typeparam>
	/// <param name="func">The function to invoke for each set of matching entity/component arrays.</param>
	template<typename ...T>
	inline void iterate(const typename Identity<eastl::function<void(size_t, const EntityID *, T*...)>>::type &func);

	/// <summary>
	/// Gets a singleton component.
	/// </summary>
	/// <typeparam name="T">The type of the singleton component to get.</typeparam>
	/// <returns>The requested singleton component.</returns>
	template<typename T>
	inline T *getSingletonComponent() noexcept;

	/// <summary>
	/// Gets a singleton component.
	/// </summary>
	/// <typeparam name="T">The type of the singleton component to get.</typeparam>
	/// <returns>The requested singleton component.</returns>
	template<typename T>
	inline const T *getSingletonComponent() const noexcept;

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
	inline bool isRegisteredComponent() const noexcept;
	template<typename ...T>
	inline bool isNotSingletonComponent() const noexcept;
	bool isRegisteredComponent(size_t count, const ComponentID *componentIDs) const noexcept;
	bool isNotSingletonComponent(size_t count, const ComponentID *componentIDs) const noexcept;

	EntityID createEntityInternal(size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept;
	void addComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs, const void *const *componentData, ComponentConstructorType constructorType) noexcept;
	bool removeComponentsInternal(EntityID entity, size_t componentCount, const ComponentID *componentIDs) noexcept;
	Archetype *findOrCreateArchetype(const ComponentMask &mask) noexcept;
};

#include "ECS.inl"