#pragma once
#include "ECS.h"
#include "reflection/TypeInfo.h"

class ECSTypeIDTranslator
{
public:
	template<typename T>
	inline static void registerType() noexcept
	{
		auto compID = ComponentIDGenerator::getID<T>();
		auto typeID = getTypeID<T>();
		s_fromComponentID[compID] = typeID;
		s_fromTypeID[typeID] = compID;
	}

	inline static ComponentID getFromTypeID(const TypeID &typeID) noexcept
	{
		assert(s_fromTypeID.find(typeID) != s_fromTypeID.end());
		return s_fromTypeID[typeID];
	}

	inline static TypeID getFromComponentID(ComponentID componentID) noexcept
	{
		assert(s_fromComponentID.find(componentID) != s_fromComponentID.end());
		return s_fromComponentID[componentID];
	}

private:
	static eastl::hash_map<ComponentID, TypeID> s_fromComponentID;
	static eastl::hash_map<TypeID, ComponentID, UUIDHash> s_fromTypeID;
};