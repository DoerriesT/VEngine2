#pragma once

struct lua_State;
class AnimationGraphInstance;

namespace AnimationGraphInstanceLua
{
	void open(lua_State *L);
	void createInstance(lua_State *L, AnimationGraphInstance *graphInstance);
}