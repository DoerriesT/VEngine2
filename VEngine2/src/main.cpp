#include "Engine.h"
#include <EASTL/fixed_vector.h>

void *__cdecl operator new[](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

void *__cdecl operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char *pName, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}


int main(int argc, char *argv[])
{
	eastl::fixed_vector<int, 10> v;

	for (int i = 0; i < 10; ++i)
	{
		v.push_back(i);
	}

	Engine engine;
	return engine.start(argc, argv);
}