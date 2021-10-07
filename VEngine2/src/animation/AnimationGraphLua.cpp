#include "AnimationGraphLua.h"
#include "AnimationGraph.h"
#include "script/lua-5.4.3/lua.hpp"
#include "script/LuaUtil.h"

static const char *k_animationGraphMetaTableName = "VEngine2.AnimationGraph";

static AnimationGraph *checkAnimationGraph(lua_State *L) noexcept
{
	void *userdata = luaL_checkudata(L, 1, k_animationGraphMetaTableName);
	luaL_argcheck(L, userdata != nullptr, 1, "'AnimationGraph' expected");
	return *static_cast<AnimationGraph **>(userdata);
}

static int luaAnimationGraphSetFloatParam(lua_State *L) noexcept
{
	AnimationGraph *graph = checkAnimationGraph(L);
	const char *name = luaL_checkstring(L, 2);
	float value = (float)luaL_checknumber(L, 3);

	graph->setFloatParam(StringID(name), value);

	return 0;
}

static int luaAnimationGraphSetIntParam(lua_State *L) noexcept
{
	AnimationGraph *graph = checkAnimationGraph(L);
	const char *name = luaL_checkstring(L, 2);
	int32_t value = (int32_t)luaL_checkinteger(L, 3);

	graph->setIntParam(StringID(name), value);

	return 0;
}

static int luaAnimationGraphSetBoolParam(lua_State *L) noexcept
{
	AnimationGraph *graph = checkAnimationGraph(L);
	const char *name = luaL_checkstring(L, 2);
	bool value = (bool)luaL_checkinteger(L, 3);

	graph->setBoolParam(StringID(name), value);

	return 0;
}

static int luaAnimationGraphGetFloatParam(lua_State *L) noexcept
{
	AnimationGraph *graph = checkAnimationGraph(L);
	const char *name = luaL_checkstring(L, 2);
	float value = 0.0f;

	bool result = graph->getFloatParam(StringID(name), &value);

	lua_pushnumber(L, (lua_Number)value);
	lua_pushboolean(L, result);

	return 2;
}

static int luaAnimationGraphGetIntParam(lua_State *L) noexcept
{
	AnimationGraph *graph = checkAnimationGraph(L);
	const char *name = luaL_checkstring(L, 2);
	int32_t value = 0;

	bool result = graph->getIntParam(StringID(name), &value);

	lua_pushinteger(L, (lua_Integer)value);
	lua_pushboolean(L, result);

	return 2;
}

static int luaAnimationGraphGetBoolParam(lua_State *L) noexcept
{
	AnimationGraph *graph = checkAnimationGraph(L);
	const char *name = luaL_checkstring(L, 2);
	bool value = false;

	bool result = graph->getBoolParam(StringID(name), &value);

	lua_pushboolean(L, value);
	lua_pushboolean(L, result);

	return 2;
}

void AnimationGraphLua::open(lua_State *L)
{
	const luaL_Reg metamethods[] =
	{
		{"setFloatParam", luaAnimationGraphSetFloatParam},
		{"setIntParam", luaAnimationGraphSetIntParam},
		{"setBoolParam", luaAnimationGraphSetBoolParam},
		{"getFloatParam", luaAnimationGraphGetFloatParam},
		{"getIntParam", luaAnimationGraphGetIntParam},
		{"getBoolParam", luaAnimationGraphGetBoolParam},
		{NULL, NULL}
	};

	luaL_newmetatable(L, k_animationGraphMetaTableName);

	lua_pushvalue(L, -1); // duplicate the meta table
	lua_setfield(L, -2, "__index"); // metatable.__index = metatable
	luaL_setfuncs(L, metamethods, 0); // register metamethods
	lua_pop(L, 1);
}

void AnimationGraphLua::createInstance(lua_State *L, AnimationGraph *graph)
{
	AnimationGraph **ppGraph = (AnimationGraph **)lua_newuserdata(L, sizeof(graph));
	*ppGraph = graph;

	luaL_getmetatable(L, k_animationGraphMetaTableName);
	lua_setmetatable(L, -2);
}
