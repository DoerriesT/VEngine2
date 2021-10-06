#pragma once
#include <stdint.h>
#include "lua-5.4.3/lua.hpp"

namespace LuaUtil
{
	struct LuaValue
	{
		enum Type
		{
			NUMBER, INTEGER, STRING, FUNCTION, BOOL, LIGHT_USER_DATA
		};

		union Value
		{
			lua_Number m_number;
			lua_Integer m_integer;
			const char *m_string;
			lua_CFunction m_function;
			bool m_bool;
			void *m_lightUserData;
		};

		Type m_type;
		Value m_value;

		static LuaValue makeNumberValue(lua_Number value) noexcept;
		static LuaValue makeIntegerValue(lua_Integer value) noexcept;
		static LuaValue makeStringValue(const char *value) noexcept;
		static LuaValue makeFunctionValue(lua_CFunction value) noexcept;
		static LuaValue makeBoolValue(bool value) noexcept;
		static LuaValue makeLightUserDataValue(void *value) noexcept;
	};

	void stackDump(lua_State *L) noexcept;

	lua_Number getTableNumberField(lua_State *L, const char *key) noexcept;
	lua_Integer getTableIntegerField(lua_State *L, const char *key) noexcept;
	const char *getTableStringField(lua_State *L, const char *key) noexcept;
	bool getTableBoolField(lua_State *L, const char *key) noexcept;
	void *getTableLightUserDataField(lua_State *L, const char *key) noexcept;

	/// <summary>
	/// Sets the requested field of the table to nil.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	void setTableNilField(lua_State *L, const char *key) noexcept;

	/// <summary>
	/// Sets the requested field of the table to the given value.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	/// <param name="value">The value to set the field to.</param>
	void setTableNumberField(lua_State *L, const char *key, lua_Number value) noexcept;

	/// <summary>
	/// Sets the requested field of the table to the given value.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	/// <param name="value">The value to set the field to.</param>
	void setTableIntegerField(lua_State *L, const char *key, lua_Integer value) noexcept;

	/// <summary>
	/// Sets the requested field of the table to the given value.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	/// <param name="value">The value to set the field to.</param>
	void setTableStringField(lua_State *L, const char *key, const char *value) noexcept;

	/// <summary>
	/// Sets the requested field of the table to the given value.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	/// <param name="value">The value to set the field to.</param>
	/// <param name="numUpValues">The number of upvalues (captured arguments available to the C-function when called) of the new closure.</param>
	/// <param name="upvalues">An array of upvalues.</param>
	void setTableCClosureField(lua_State *L, const char *key, lua_CFunction value, int numUpvalues = 0, const LuaValue *upvalues = nullptr) noexcept;

	/// <summary>
	/// Sets the requested field of the table to the given value.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	/// <param name="value">The value to set the field to.</param>
	void setTableBoolField(lua_State *L, const char *key, bool value) noexcept;

	/// <summary>
	/// Sets the requested field of the table to the given value.
	/// Assumes that the table is at the top of the stack.
	/// </summary>
	/// <param name="L">The lua state.</param>
	/// <param name="key">The key to index into the table.</param>
	/// <param name="value">The value to set the field to.</param>
	void setTableLightUserDataField(lua_State *L, const char *key, void *value) noexcept;
}