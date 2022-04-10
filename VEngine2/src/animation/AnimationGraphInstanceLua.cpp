#include "AnimationGraphInstanceLua.h"
#include "AnimationGraphInstance.h"
#include "script/lua-5.4.3/lua.hpp"
#include "script/LuaUtil.h"

static const char *k_animationGraphInstanceMetaTableName = "VEngine2.AnimationGraphInstance";

static AnimationGraphInstance *checkAnimationGraphInstance(lua_State *L) noexcept
{
	void *userdata = luaL_checkudata(L, 1, k_animationGraphInstanceMetaTableName);
	luaL_argcheck(L, userdata != nullptr, 1, "'AnimationGraphInstance' expected");
	return *static_cast<AnimationGraphInstance **>(userdata);
}

static int luaAnimationGraphInstanceSetFloatParam(lua_State *L) noexcept
{
	AnimationGraphInstance *graphInstance = checkAnimationGraphInstance(L);
	const char *name = luaL_checkstring(L, 2);
	float value = (float)luaL_checknumber(L, 3);

	graphInstance->setFloatParam(StringID(name), value);

	return 0;
}

static int luaAnimationGraphInstanceSetIntParam(lua_State *L) noexcept
{
	AnimationGraphInstance *graphInstance = checkAnimationGraphInstance(L);
	const char *name = luaL_checkstring(L, 2);
	int32_t value = (int32_t)luaL_checkinteger(L, 3);

	graphInstance->setIntParam(StringID(name), value);

	return 0;
}

static int luaAnimationGraphInstanceSetBoolParam(lua_State *L) noexcept
{
	AnimationGraphInstance *graphInstance = checkAnimationGraphInstance(L);
	const char *name = luaL_checkstring(L, 2);
	bool value = (bool)luaL_checkinteger(L, 3);

	graphInstance->setBoolParam(StringID(name), value);

	return 0;
}

static int luaAnimationGraphInstanceGetFloatParam(lua_State *L) noexcept
{
	AnimationGraphInstance *graphInstance = checkAnimationGraphInstance(L);
	const char *name = luaL_checkstring(L, 2);
	float value = 0.0f;

	bool result = graphInstance->getFloatParam(StringID(name), &value);

	lua_pushnumber(L, (lua_Number)value);
	lua_pushboolean(L, result);

	return 2;
}

static int luaAnimationGraphInstanceGetIntParam(lua_State *L) noexcept
{
	AnimationGraphInstance *graphInstance = checkAnimationGraphInstance(L);
	const char *name = luaL_checkstring(L, 2);
	int32_t value = 0;

	bool result = graphInstance->getIntParam(StringID(name), &value);

	lua_pushinteger(L, (lua_Integer)value);
	lua_pushboolean(L, result);

	return 2;
}

static int luaAnimationGraphInstanceGetBoolParam(lua_State *L) noexcept
{
	AnimationGraphInstance *graphInstance = checkAnimationGraphInstance(L);
	const char *name = luaL_checkstring(L, 2);
	bool value = false;

	bool result = graphInstance->getBoolParam(StringID(name), &value);

	lua_pushboolean(L, value);
	lua_pushboolean(L, result);

	return 2;
}

void AnimationGraphInstanceLua::open(lua_State *L)
{
	const luaL_Reg metamethods[] =
	{
		{"setFloatParam", luaAnimationGraphInstanceSetFloatParam},
		{"setIntParam", luaAnimationGraphInstanceSetIntParam},
		{"setBoolParam", luaAnimationGraphInstanceSetBoolParam},
		{"getFloatParam", luaAnimationGraphInstanceGetFloatParam},
		{"getIntParam", luaAnimationGraphInstanceGetIntParam},
		{"getBoolParam", luaAnimationGraphInstanceGetBoolParam},
		{NULL, NULL}
	};

	luaL_newmetatable(L, k_animationGraphInstanceMetaTableName);

	lua_pushvalue(L, -1); // duplicate the meta table
	lua_setfield(L, -2, "__index"); // metatable.__index = metatable
	luaL_setfuncs(L, metamethods, 0); // register metamethods
	lua_pop(L, 1);
}

void AnimationGraphInstanceLua::createInstance(lua_State *L, AnimationGraphInstance *graphInstance)
{
	AnimationGraphInstance **ppGraphInstance = (AnimationGraphInstance **)lua_newuserdata(L, sizeof(graphInstance));
	*ppGraphInstance = graphInstance;

	luaL_getmetatable(L, k_animationGraphInstanceMetaTableName);
	lua_setmetatable(L, -2);
}
