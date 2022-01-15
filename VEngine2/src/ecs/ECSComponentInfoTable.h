#pragma once
#include "ECS.h"

struct lua_State;
struct TransformComponent;
class Renderer;

struct ECSComponentInfo
{
	ErasedType m_erasedType;
	void (*m_onGUI)(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept;
	void (*m_toLua)(lua_State *L, void *instance) noexcept;
	void (*m_fromLua)(lua_State *L, void *instance) noexcept;
	const char *m_name;
	const char *m_displayName;
	const char *m_tooltip;
};

class ECSComponentInfoTable
{
public:
	template<typename T>
	inline static void registerType() noexcept
	{
		auto compID = ComponentIDGenerator::getID<T>();

		ECSComponentInfo info{};
		info.m_erasedType = ErasedType::create<T>();
		info.m_onGUI = T::onGUI;
		info.m_toLua = T::toLua;
		info.m_fromLua = T::fromLua;
		info.m_name = T::getComponentName();
		info.m_displayName = T::getComponentDisplayName();
		info.m_displayName = info.m_displayName ? info.m_displayName : info.m_name;
		info.m_tooltip = T::getComponentTooltip();

		s_ecsComponentInfo[compID] = info;

		if (static_cast<int64_t>(compID) > s_highestRegisteredComponentID)
		{
			s_highestRegisteredComponentID = static_cast<int64_t>(compID);
		}
	}

	inline static const ECSComponentInfo &getComponentInfo(ComponentID componentID) noexcept
	{
		return s_ecsComponentInfo[componentID];
	}

	inline static int64_t getHighestRegisteredComponentIDValue() noexcept
	{
		return s_highestRegisteredComponentID;
	}

private:
	static ECSComponentInfo s_ecsComponentInfo[k_ecsMaxComponentTypes];
	static int64_t s_highestRegisteredComponentID;
};