#pragma once
#include <stdint.h>
#include <EASTL/functional.h>
#include <EASTL/vector.h>
#include "ECSCommon.h"
#include "utility/ErasedType.h"
#include "utility/DeletedCopyMove.h"

class Archetype;

/// <summary>
/// Describes a memory location for storing an entity and its components inside an Archetype.
/// </summary>
struct ArchetypeSlot
{
	uint32_t m_chunkIdx = 0;
	uint32_t m_chunkSlotIdx = 0;
};

/// <summary>
/// Describes an entities Archetype and the memory location inside the Archetype.
/// </summary>
struct EntityRecord
{
	Archetype *m_archetype = nullptr;
	ArchetypeSlot m_slot = {};
};

/// <summary>
/// A chunk of memory for storing k_ecsComponentsPerMemoryChunk entities and components in consecutive arrays.
/// </summary>
struct ArchetypeMemoryChunk
{
	uint8_t *m_memory = nullptr;
	size_t m_size = 0;
};

/// <summary>
/// Iterates through all set bits in the given ComponentMask and invokes the given callback with the index
/// of the call (index starts at 0 and is incremented for each call of the callback) and the ComponentID corresponding
/// to the set bit.
/// </summary>
/// <param name="mask">The ComponentMask to iterate over.</param>
/// <param name="f">A callback to invoke on each set bit.</param>
void forEachComponentType(const ComponentMask &mask, const eastl::function<void(size_t index, ComponentID componentID)> &f) noexcept;

/// <summary>
/// An Archetype has blocks of memory for storing all entities (and their components) of the same type. Entities are of the same type
/// if they have the same set of components attached to them. Entities and components are stored SoA style so that there is an individual
/// array for each component type.
/// The ECS has an Archetype for each unique set of components that are used on an entity.
/// </summary>
class Archetype
{
public:

	/// <summary>
	/// Constructs a new Archetype for a given set of component types.
	/// </summary>
	/// <param name="componentMask">The ComponentMask defining this Archetype.</param>
	/// <param name="componentInfo">An array of ErasedType corresponding to each bit of the ComponentMask.</param>
	explicit Archetype(const ComponentMask &componentMask, const ErasedType *componentInfo) noexcept;

	DELETED_COPY_MOVE(Archetype);

	~Archetype() noexcept;

	/// <summary>
	/// Gets the ComponentMask defining this Archetype.
	/// </summary>
	/// <returns>This Archetypes ComponentMask.</returns>
	const ComponentMask &getComponentMask() const noexcept;

	/// <summary>
	/// Gets the vector of ArchetypeMemoryChunks. 
	/// </summary>
	/// <returns>The vector of ArchetypeMemoryChunks</returns>
	const eastl::vector<ArchetypeMemoryChunk> &getMemoryChunks() noexcept;

	/// <summary>
	/// Gets the byte offset into memory chunks where the array of components of the given type starts.
	/// </summary>
	/// <param name="componentID">The component type to get the offset for.</param>
	/// <returns>The byte offset into memory chunks where the array of components of the given type starts.</returns>
	size_t getComponentArrayOffset(ComponentID componentID) const noexcept;

	/// <summary>
	/// Allocates a slot for storing an entity and its components. Does not call constructors.
	/// </summary>
	/// <returns>The newly allocated slot.</returns>
	ArchetypeSlot allocateDataSlot() noexcept;

	/// <summary>
	/// Frees a previously allocated slot. Does not call destructors on the to be freed component memory.
	/// </summary>
	/// <param name="slot">The slot to free.</param>
	void freeDataSlot(const ArchetypeSlot &slot) noexcept;

	/// <summary>
	/// Migrates an entity and all matching components from another Archetype to this one.
	/// Components that are shared between both Archetypes are moved.
	/// Components present in the old Archetype but not in this one are ignored.
	/// Components present in this Archetype but not in the old one are optionally default constructed.
	/// Calls destructors on the components of the old Archetype.
	/// </summary>
	/// <param name="entity">The entity to migrate.</param>
	/// <param name="oldRecord">The old Archetype and slot of the entity.</param>
	/// <param name="constructorsToSkip">An optional pointer to a mask of all components whose constructor should be skipped.
	/// The caller *must* then call constructors at the appropriate memory addresses to ensure that all components are in a valid state.</param>
	/// <returns>The new EntityRecord of the entity specifying this Archetype and the entities slot.</returns>
	EntityRecord migrate(EntityID entity, const EntityRecord &oldRecord, ComponentMask *constructorsToSkip = nullptr) noexcept;

	/// <summary>
	/// Calls destructors on all components of the given slot. This assumes that all components are in a valid (constructed) state.
	/// Components may be in a not-constructed state if migrate() was called with a mask of components for which to skip the constructor.
	/// In this case, the caller is responsible for calling a constructor at the appropriate memory addresses before calling this function.
	/// Calling this function twice is also a bad idea.
	/// </summary>
	/// <param name="slot">The slot to call destructors on.</param>
	/// <returns></returns>
	void callDestructors(const ArchetypeSlot &slot) noexcept;

	/// <summary>
	/// Gets a pointer to the memory of a component of a given entity.
	/// </summary>
	/// <param name="slot">The slot of the entity.</param>
	/// <param name="componentID">The type of the component.</param>
	/// <returns>A pointer to the memory of the component of the given entity.</returns>
	uint8_t *getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) noexcept;

	/// <summary>
	/// Gets a pointer to the memory of a component of a given entity.
	/// </summary>
	/// <param name="slot">The slot of the entity.</param>
	/// <param name="componentID">The type of the component.</param>
	/// <returns>A pointer to the memory of the component of the given entity.</returns>
	const uint8_t *getComponentMemory(const ArchetypeSlot &slot, ComponentID componentID) const noexcept;

private:
	ComponentMask m_componentMask = {};
	const ErasedType *m_componentInfo = nullptr;
	size_t m_memoryChunkSize = 0;
	eastl::vector<ArchetypeMemoryChunk> m_memoryChunks;
	eastl::vector<size_t> m_componentArrayOffsets;
};