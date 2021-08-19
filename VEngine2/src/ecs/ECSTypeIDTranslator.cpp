#include "ECSTypeIDTranslator.h"

eastl::hash_map<ComponentID, TypeID> ECSTypeIDTranslator::s_fromComponentID;
eastl::hash_map<TypeID, ComponentID, UUIDHash> ECSTypeIDTranslator::s_fromTypeID;
BasicComponentReflectionData ECSTypeIDTranslator::s_basicReflectionData[k_ecsMaxComponentTypes];