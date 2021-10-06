#include "ECSLua.h"
#include "script/lua-5.4.3/lua.hpp"
#include "ECS.h"
#include "ECSComponentInfoTable.h"
#include "script/LuaUtil.h"
#include "utility/Memory.h"

static const char *k_ecsMetaTableName = "VEngine2.ECS";

static ECS *checkECS(lua_State *L) noexcept
{
	void *userdata = luaL_checkudata(L, 1, k_ecsMetaTableName);
	luaL_argcheck(L, userdata != nullptr, 1, "'ECS' expected");
	return *static_cast<ECS **>(userdata);
}

static int luaECSCreateEntity(lua_State *L) noexcept
{
	const auto argCount = lua_gettop(L);
	ECS *ecs = checkECS(L);

	const size_t componentCount = argCount - 1; // first arg is ecs
	ComponentID *componentIDs = ALLOC_A_T(ComponentID, componentCount);

	for (size_t i = 0; i < componentCount; ++i)
	{
		componentIDs[i] = (ComponentID)luaL_checkinteger(L, 2 + (int)i);
	}

	EntityID entity = ecs->createEntityTypeless(componentCount, componentIDs);
	lua_pushinteger(L, (lua_Integer)entity);

	return 1;
}

static int luaECSDestroyEntity(lua_State *L) noexcept
{
	ECS *ecs = checkECS(L);
	EntityID entity = (EntityID)luaL_checkinteger(L, 2);

	ecs->destroyEntity(entity);

	return 0;
}

static int luaECSAddComponents(lua_State *L) noexcept
{
	const auto argCount = lua_gettop(L);
	ECS *ecs = checkECS(L);
	EntityID entity = (EntityID)luaL_checkinteger(L, 2);

	const size_t componentCount = argCount - 2; // first arg is ecs, second arg is entity

	// early exit
	if (componentCount == 0)
	{
		return 0;
	}

	ComponentID *componentIDs = ALLOC_A_T(ComponentID, componentCount);

	for (size_t i = 0; i < componentCount; ++i)
	{
		componentIDs[i] = (ComponentID)luaL_checkinteger(L, 3 + (int)i);
	}

	ecs->addComponentsTypeless(entity, componentCount, componentIDs);

	return 0;
}

static int luaECSRemoveComponents(lua_State *L) noexcept
{
	const auto argCount = lua_gettop(L);
	ECS *ecs = checkECS(L);
	EntityID entity = (EntityID)luaL_checkinteger(L, 2);

	const size_t componentCount = argCount - 2; // first arg is ecs, second arg is entity

	// early exit
	if (componentCount == 0)
	{
		lua_pushboolean(L, true);
		return 1;
	}

	ComponentID *componentIDs = ALLOC_A_T(ComponentID, componentCount);

	for (size_t i = 0; i < componentCount; ++i)
	{
		componentIDs[i] = (ComponentID)luaL_checkinteger(L, 3 + (int)i);
	}

	const bool res = ecs->removeComponentsTypeless(entity, componentCount, componentIDs);

	lua_pushboolean(L, res);

	return 1;
}

static int luaECSGetComponent(lua_State *L) noexcept
{
	ECS *ecs = checkECS(L);
	EntityID entity = (EntityID)luaL_checkinteger(L, 2);
	ComponentID compID = (ComponentID)luaL_checkinteger(L, 3);

	void *component = ecs->getComponentTypeless(entity, compID);

	if (component)
	{
		lua_newtable(L);
		ECSComponentInfoTable::getComponentInfo(compID).m_toLua(L, component);
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}

static int luaECSSetComponent(lua_State *L) noexcept
{
	ECS *ecs = checkECS(L);
	EntityID entity = (EntityID)luaL_checkinteger(L, 2);
	ComponentID compID = (ComponentID)luaL_checkinteger(L, 3);
	luaL_argcheck(L, lua_istable(L, 4), 4, "'table' expected");
	lua_settop(L, 4);

	void *component = ecs->getComponentTypeless(entity, compID);

	if (component)
	{
		ECSComponentInfoTable::getComponentInfo(compID).m_fromLua(L, component);
	}

	lua_pushboolean(L, component != nullptr);

	return 1;
}

static int luaECSHasComponents(lua_State *L) noexcept
{
	const auto argCount = lua_gettop(L);
	ECS *ecs = checkECS(L);
	EntityID entity = (EntityID)luaL_checkinteger(L, 2);

	const size_t componentCount = argCount - 2; // first arg is ecs, second arg is entity
	ComponentID *componentIDs = ALLOC_A_T(ComponentID, componentCount);

	for (size_t i = 0; i < componentCount; ++i)
	{
		componentIDs[i] = (ComponentID)luaL_checkinteger(L, 3 + (int)i);
	}

	lua_pushboolean(L, ecs->hasComponentsTypeless(entity, componentCount, componentIDs));

	return 1;
}

static int luaECSIterate(lua_State *L) noexcept
{
	const auto argCount = lua_gettop(L);
	ECS *ecs = checkECS(L);

	size_t componentCount = argCount - 2; // first arg is ecs, last arg is callback

	// early exit if iterating over 0 components
	if (componentCount == 0)
	{
		return 1;
	}

	ComponentID *componentIDs = ALLOC_A_T(ComponentID, componentCount);

	for (size_t i = 0; i < componentCount; ++i)
	{
		componentIDs[i] = (ComponentID)luaL_checkinteger(L, 2 + (int)i);
	}

	luaL_argcheck(L, lua_isfunction(L, argCount), argCount, "'function' expected");

	ecs->iterateTypeless(componentCount, componentIDs, [&](size_t entityCount, const EntityID *entities, void **components)
		{
			for (size_t i = 0; i < entityCount; ++i)
			{
				lua_pushvalue(L, -1); // create copy of function to be consumed

				// put first argument on the stack
				lua_pushinteger(L, (lua_Integer)entities[i]);

				// put component arguments on the stack
				for (size_t j = 0; j < componentCount; ++j)
				{
					// get component memory
					const auto &compInfo = ECSComponentInfoTable::getComponentInfo(componentIDs[j]);
					void *comp = (char *)components[j] + i * compInfo.m_erasedType.m_size;

					// create lua representation of component
					lua_newtable(L);
					compInfo.m_toLua(L, comp);
				}

				// call callback
				if (auto ret = lua_pcall(L, (int)componentCount + 1, 0, 0); ret != LUA_OK)
				{
					luaL_argcheck(L, false, argCount, "Calling callback function failed!");
				}
			}
		});

	return 0;
}

void ECSLua::open(lua_State *L)
{
	// create ComponentIDs table
	const auto registeredComponentCount = ECSComponentInfoTable::getHighestRegisteredComponentIDValue();
	lua_newtable(L);
	for (int64_t i = 0; i <= registeredComponentCount; ++i)
	{
		if (ECSComponentInfoTable::getComponentInfo((ComponentID)i).m_name)
		{
			LuaUtil::setTableIntegerField(L, ECSComponentInfoTable::getComponentInfo((ComponentID)i).m_name, (lua_Integer)(ComponentID)i);
		}
	}
	lua_setglobal(L, "ComponentIDs");

	const luaL_Reg metamethods[] =
	{
		{"createEntity", luaECSCreateEntity},
		{"destroyEntity", luaECSDestroyEntity},
		{"addComponents", luaECSAddComponents},
		{"removeComponents", luaECSRemoveComponents},
		{"getComponent", luaECSGetComponent},
		{"setComponent", luaECSSetComponent},
		{"hasComponents", luaECSHasComponents},
		{"iterate", luaECSIterate},
		{NULL, NULL}
	};

	luaL_newmetatable(L, k_ecsMetaTableName);

	lua_pushvalue(L, -1); // duplicate the meta table
	lua_setfield(L, -2, "__index"); // metatable.__index = metatable
	luaL_setfuncs(L, metamethods, 0); // register metamethods
}

void ECSLua::createInstance(lua_State *L, ECS *ecs)
{
	ECS **ppECS = (ECS **)lua_newuserdata(L, sizeof(ecs));
	*ppECS = ecs;

	luaL_getmetatable(L, k_ecsMetaTableName);
	lua_setmetatable(L, -2);
}
