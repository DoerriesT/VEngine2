#include "Engine.h"
#include "Log.h"

void Engine::start(int argc, char *argv[])
{
	for (int i = 0; i < argc; ++i)
	{
		Log::info(argv[i]);
	}
	Log::err("Test");
}
