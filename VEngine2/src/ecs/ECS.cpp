#include "ECS.h"

Archetype::Archetype(Archetype *parent, ComponentID componentID, size_t additionalTypeSize, size_t additionalTypeAlignment) noexcept
	:m_componentMask(parent ? parent->m_componentMask : 0),
	m_memoryChunks()
{

}
