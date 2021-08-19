#pragma once
#include "ECS.h"

struct ECSComponentInfo
{
	void (*m_onGUI)(void *instance) noexcept;
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
		info.m_onGUI = T::onGUI;
		info.m_name = T::getComponentName();
		info.m_displayName = T::getComponentDisplayName();
		info.m_displayName = info.m_displayName ? info.m_displayName : info.m_name;
		info.m_tooltip = T::getComponentTooltip();

		s_ecsComponentInfo[compID] = info;
	}

	inline static const ECSComponentInfo &getComponentInfo(ComponentID componentID) noexcept
	{
		return s_ecsComponentInfo[componentID];
	}

private:
	static ECSComponentInfo s_ecsComponentInfo[k_ecsMaxComponentTypes];
};