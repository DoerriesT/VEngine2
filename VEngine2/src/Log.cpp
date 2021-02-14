#include "Log.h"
#include "stdio.h"

void Log::info(const char *message)
{
	printf("INFO: ");
	puts(message);
}

void Log::warn(const char *message)
{
	printf("WARNING: ");
	puts(message);
}

void Log::err(const char *message)
{
	printf("ERROR: ");
	puts(message);
}
