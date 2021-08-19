#pragma once
#include "ECS.h"
#include "reflection/TypeInfo.h"

struct BasicComponentReflectionData
{
	void (*m_onGUI)(void *instance) noexcept;
	const char *m_name;
	const char *m_displayName;
	const char *m_tooltip;
};

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

		BasicComponentReflectionData reflData{};
		reflData.m_onGUI = T::onGUI;
		reflData.m_name = T::getComponentName();
		reflData.m_displayName = T::getComponentDisplayName();
		reflData.m_displayName = reflData.m_displayName ? reflData.m_displayName : reflData.m_name;
		reflData.m_tooltip = T::getComponentTooltip();

		s_basicReflectionData[compID] = reflData;
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

	inline static const BasicComponentReflectionData &getBasicComponentReflectionData(ComponentID componentID) noexcept
	{
		return s_basicReflectionData[componentID];
	}

private:
	static eastl::hash_map<ComponentID, TypeID> s_fromComponentID;
	static eastl::hash_map<TypeID, ComponentID, UUIDHash> s_fromTypeID;
	static BasicComponentReflectionData s_basicReflectionData[k_ecsMaxComponentTypes];
};