#include "Log.h"
#include <stdio.h>
#include <stdarg.h>
#include <EASTL/vector.h>

static eastl::vector<char> formatIntoVector(const char *fmt, va_list args)
{
    // make a copy
    va_list argsCopy;
    va_copy(argsCopy, args);

    // get required size
    int requiredSize = vsnprintf(nullptr, 0, fmt, argsCopy) + 1 /*null terminator*/;

    // clean up copy
    va_end(argsCopy);

    // allocate memory
    eastl::vector<char> buffer(requiredSize);

    // format
    vsnprintf(buffer.data(), buffer.size(), fmt, args);

    return buffer;
}

void Log::info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    auto buffer = formatIntoVector(fmt, args);

    // clean up varargs
    va_end(args);

    printf("INFO: %s\n", buffer.data());
}

void Log::warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    auto buffer = formatIntoVector(fmt, args);

    // clean up varargs
    va_end(args);

    printf("WARNING: %s\n", buffer.data());
}

void Log::err(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    auto buffer = formatIntoVector(fmt, args);

    // clean up varargs
    va_end(args);

    printf("ERROR: %s\n", buffer.data());
}
