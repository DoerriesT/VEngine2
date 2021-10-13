#include "LuaUtil.h"
#include "lua-5.4.3/lua.hpp"
#include <assert.h>

void LuaUtil::stackDump(lua_State *L) noexcept
{
	printf("Lua Stack:\n");
	int i;
	int top = lua_gettop(L);
	for (i = 1; i <= top; i++) /* repeat for each level */
	{
		int t = lua_type(L, i);
		switch (t)
		{
		case LUA_TSTRING:  /* strings */
			printf("`%s'\n", lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			printf(lua_toboolean(L, i) ? "true\n" : "false\n");
			break;

		case LUA_TNUMBER:  /* numbers */
			printf("%g\n", lua_tonumber(L, i));
			break;

		default:  /* other values */
			printf("%s\n", lua_typename(L, t));
			break;

		}
	}
	printf("\n");  /* end the listing */
}

lua_Number LuaUtil::getTableNumberField(lua_State *L, const char *key) noexcept
{
	int type = lua_getfield(L, -1, key);
	assert(type == LUA_TNUMBER);
	auto val = lua_tonumber(L, -1);
	lua_pop(L, 1);
	return val;
}

lua_Integer LuaUtil::getTableIntegerField(lua_State *L, const char *key) noexcept
{
	int type = lua_getfield(L, -1, key);
	assert(type == LUA_TNUMBER);
	auto val = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return val;
}

const char *LuaUtil::getTableStringField(lua_State *L, const char *key) noexcept
{
	int type = lua_getfield(L, -1, key);
	assert(type == LUA_TSTRING);
	const char *val = lua_tostring(L, -1);
	lua_pop(L, 1);
	return val;
}

bool LuaUtil::getTableBoolField(lua_State *L, const char *key) noexcept
{
	int type = lua_getfield(L, -1, key);
	assert(type == LUA_TBOOLEAN);
	auto val = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return val;
}

void *LuaUtil::getTableLightUserDataField(lua_State *L, const char *key) noexcept
{
	int type = lua_getfield(L, -1, key);
	assert(type == LUA_TLIGHTUSERDATA);
	auto val = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return val;
}

void LuaUtil::getTableNumberArrayField(lua_State *L, const char *key, size_t arraySize, float *result) noexcept
{
	// get table with array
	int type = lua_getfield(L, -1, key);
	assert(type == LUA_TTABLE);

	// iterate over array elements in table
	for (size_t i = 0; i < arraySize; ++i)
	{
		// push array element to stack
		type = lua_rawgeti(L, -1, (lua_Integer)i);
		assert(type == LUA_TNUMBER);
		result[i] = (float)lua_tonumber(L, -1);
		// pop array element
		lua_pop(L, 1);
	}
	
	// pop table
	lua_pop(L, 1);
}

void LuaUtil::setTableNilField(lua_State *L, const char *key) noexcept
{
	lua_pushnil(L);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableNumberField(lua_State *L, const char *key, lua_Number value) noexcept
{
	lua_pushnumber(L, value);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableIntegerField(lua_State *L, const char *key, lua_Integer value) noexcept
{
	lua_pushinteger(L, value);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableStringField(lua_State *L, const char *key, const char *value) noexcept
{
	lua_pushstring(L, value);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableCClosureField(lua_State *L, const char *key, lua_CFunction value, int numUpvalues, const LuaValue *upvalues) noexcept
{
	// push upvalues
	for (size_t i = 0; i < numUpvalues; ++i)
	{
		switch (upvalues[i].m_type)
		{
		case LuaValue::NUMBER:
			lua_pushnumber(L, upvalues[i].m_value.m_number); break;
		case LuaValue::INTEGER:
			lua_pushinteger(L, upvalues[i].m_value.m_integer); break;
		case LuaValue::STRING: 
			lua_pushstring(L, upvalues[i].m_value.m_string); break;
		case LuaValue::FUNCTION:
			lua_pushcfunction(L, upvalues[i].m_value.m_function); break;
		case LuaValue::BOOL:
			lua_pushboolean(L, upvalues[i].m_value.m_bool); break;
		case LuaValue::LIGHT_USER_DATA:
			lua_pushlightuserdata(L, upvalues[i].m_value.m_lightUserData); break;
		default:
			assert(false);
			break;
		}
	}

	lua_pushcclosure(L, value, numUpvalues);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableBoolField(lua_State *L, const char *key, bool value) noexcept
{
	lua_pushboolean(L, value);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableLightUserDataField(lua_State *L, const char *key, void *value) noexcept
{
	lua_pushlightuserdata(L, value);
	lua_setfield(L, -2, key);
}

void LuaUtil::setTableNumberArrayField(lua_State *L, const char *key, const float *value, size_t arraySize) noexcept
{
	lua_createtable(L, arraySize, 0);
	for (size_t i = 0; i < arraySize; ++i)
	{
		lua_pushnumber(L, (lua_Number)value[i]);
		lua_rawseti(L, -2, (lua_Integer)i);
	}
	lua_setfield(L, -2, key);
}

LuaUtil::LuaValue LuaUtil::LuaValue::makeNumberValue(lua_Number value) noexcept
{
	LuaUtil::LuaValue luaVal;
	luaVal.m_type = LuaUtil::LuaValue::NUMBER;
	luaVal.m_value.m_number = value;
	return luaVal;
}

LuaUtil::LuaValue LuaUtil::LuaValue::makeIntegerValue(lua_Integer value) noexcept
{
	LuaUtil::LuaValue luaVal;
	luaVal.m_type = LuaUtil::LuaValue::INTEGER;
	luaVal.m_value.m_integer = value;
	return luaVal;
}

LuaUtil::LuaValue LuaUtil::LuaValue::makeStringValue(const char *value) noexcept
{
	LuaUtil::LuaValue luaVal;
	luaVal.m_type = LuaUtil::LuaValue::STRING;
	luaVal.m_value.m_string = value;
	return luaVal;
}

LuaUtil::LuaValue LuaUtil::LuaValue::makeFunctionValue(lua_CFunction value) noexcept
{
	LuaUtil::LuaValue luaVal;
	luaVal.m_type = LuaUtil::LuaValue::FUNCTION;
	luaVal.m_value.m_function = value;
	return luaVal;
}

LuaUtil::LuaValue LuaUtil::LuaValue::makeBoolValue(bool value) noexcept
{
	LuaUtil::LuaValue luaVal;
	luaVal.m_type = LuaUtil::LuaValue::BOOL;
	luaVal.m_value.m_bool = value;
	return luaVal;
}

LuaUtil::LuaValue LuaUtil::LuaValue::makeLightUserDataValue(void *value) noexcept
{
	LuaUtil::LuaValue luaVal;
	luaVal.m_type = LuaUtil::LuaValue::LIGHT_USER_DATA;
	luaVal.m_value.m_lightUserData = value;
	return luaVal;
}
