#pragma once

struct lua_State;
class ECS;

namespace ECSLua
{
	void open(lua_State *L);
	void createInstance(lua_State *L, ECS *ecs);
}