#include "ScriptComponent.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include "script/LuaUtil.h"

ScriptComponent::ScriptComponent(const ScriptComponent &other) noexcept
	:m_script(other.m_script)
{
}

ScriptComponent::ScriptComponent(ScriptComponent &&other) noexcept
	:m_script(other.m_script)
{
	m_L = other.m_L;
	other.m_L = nullptr;
}

ScriptComponent &ScriptComponent::operator=(const ScriptComponent &other) noexcept
{
	if (this != &other)
	{
		if (m_L)
		{
			lua_close(m_L);
			m_L = nullptr;
		}
		m_script = other.m_script;
	}

	return *this;
}

ScriptComponent &ScriptComponent::operator=(ScriptComponent &&other) noexcept
{
	if (this != &other)
	{
		if (m_L)
		{
			lua_close(m_L);
			m_L = nullptr;
		}
		m_script = other.m_script;
		m_L = other.m_L;
		other.m_L = nullptr;
	}

	return *this;
}

ScriptComponent::~ScriptComponent() noexcept
{
	if (m_L)
	{
		lua_close(m_L);
		m_L = nullptr;
	}
}

void ScriptComponent::onGUI(void *instance) noexcept
{
	ScriptComponent &c = *reinterpret_cast<ScriptComponent *>(instance);

	AssetData *resultAssetData = nullptr;

	if (ImGuiHelpers::AssetPicker("Script Asset", ScriptAssetData::k_assetType, c.m_script.get(), &resultAssetData))
	{
		c.m_script = resultAssetData;

		if (c.m_L)
		{
			lua_close(c.m_L);
			c.m_L = nullptr;
		}
	}

	if (c.m_L)
	{
		ImGui::TextUnformatted("Properties");
		ImGui::Separator();

		auto &L = c.m_L;
		int type = lua_getfield(L, -1, "Properties");
		if (type == LUA_TTABLE)
		{
			lua_pushnil(L); // first key
			while (lua_next(L, -2))
			{
				// key is at -2, value is at -1

				if (lua_type(L, -2) == LUA_TSTRING)
				{
					const char *key = lua_tostring(L, -2);

					if (lua_isboolean(L, -1))
					{
						bool value = lua_toboolean(L, -1);
						if (ImGui::Checkbox(key, &value))
						{
							lua_pushboolean(L, value);
							lua_setfield(L, -4, key);
						}
					}
					else if (lua_isinteger(L, -1))
					{
						int value = (int)lua_tointeger(L, -1);
						if (ImGui::InputInt(key, &value))
						{
							lua_pushinteger(L, (lua_Integer)value);
							lua_setfield(L, -4, key);
						}
					}
					else if (lua_isnumber(L, -1))
					{
						double value = (double)lua_tonumber(L, -1);
						if (ImGui::InputDouble(key, &value))
						{
							lua_pushnumber(L, (lua_Number)value);
							lua_setfield(L, -4, key);
						}
					}
				}

				// removes value, keeps key for the next iteration
				lua_pop(L, 1);
			}
		}
		// remove the Properties table
		lua_pop(L, 1);
	}
}

void ScriptComponent::toLua(lua_State *L, void *instance) noexcept
{
	// TODO: nothing?
}

void ScriptComponent::fromLua(lua_State *L, void *instance) noexcept
{
	// TODO: nothing?
}
