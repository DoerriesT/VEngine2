#include "ScriptSystem.h"
#include "ecs/ECS.h"
#include "ecs/ECSLua.h"
#include "LuaUtil.h"
#include "profiling/Profiling.h"
#include "component/ScriptComponent.h"
#include "Log.h"
#include "asset/AssetManager.h"

ScriptSystem::ScriptSystem(ECS *ecs) noexcept
	:m_ecs(ecs)
{
}

ScriptSystem::~ScriptSystem() noexcept
{
}

void ScriptSystem::update(float deltaTime) noexcept
{
	PROFILING_ZONE_SCOPED;

	m_ecs->iterate<ScriptComponent>([this, deltaTime](size_t count, const EntityID *entities, ScriptComponent *scriptC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &sc = scriptC[i];

				if (!sc.m_script)
				{
					continue;
				}

				if (sc.m_script->isReloadedAssetAvailable())
				{
					if (sc.m_L)
					{
						lua_close(sc.m_L);
						sc.m_L = nullptr;
					}
					sc.m_script = AssetManager::get()->getAsset<ScriptAsset>(sc.m_script->getAssetID());
				}

				if (!sc.m_script.isLoaded())
				{
					continue;
				}

				auto &L = sc.m_L;

				// initialize lua state
				if (!L)
				{
					L = luaL_newstate();
					luaL_openlibs(L);
					ECSLua::open(L);

					if (luaL_dostring(L, sc.m_script->getScriptString()) != LUA_OK)
					{
						Log::err("ScriptSystem: Failed to load script \"%s\" with error: %s", sc.m_script->getAssetID().m_string, lua_tostring(L, lua_gettop(L)));

						lua_close(L);
						L = nullptr;

						continue;
					}
				}

				// the script has returned a table with the script functions
				// get the update() function
				lua_getfield(L, -1, "update");

				// create arguments (self,  ecs, entity, deltaTime)
				{
					// self
					lua_pushvalue(L, -2);

					// ecs
					ECSLua::createInstance(L, m_ecs);

					// entity
					lua_pushinteger(L, (lua_Integer)entities[i]);

					// delta time
					lua_pushnumber(L, (lua_Number)deltaTime);
				}

				// call update() function
				if (lua_pcall(L, 4, 0, 0) != LUA_OK)
				{
					Log::err("AnimationGraph: Failed to execute controller script \"%s\" with error: %s", sc.m_script->getAssetID().m_string, lua_tostring(L, lua_gettop(L)));
				}

				lua_settop(L, 1);
			}
		});
}
