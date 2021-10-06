#pragma once

struct lua_State;
class AnimationGraph;

namespace AnimationGraphLua
{
	void open(lua_State *L);
	void createInstance(lua_State *L, AnimationGraph *graph);
}