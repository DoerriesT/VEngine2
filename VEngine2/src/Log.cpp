#include "Log.h"
#include <stdio.h>
#include <stdarg.h>
#include <EASTL/vector.h>
#include "utility/SpinLock.h"

static char s_logScratchMem[2048];
static SpinLock s_logSpinLock;

static const char *formatV(const char *fmt, va_list args)
{
    // make a copy
    va_list argsCopy;
    va_copy(argsCopy, args);

    // get required size
    int requiredSize = vsnprintf(nullptr, 0, fmt, argsCopy) + 1 /*null terminator*/;

    // clean up copy
    va_end(argsCopy);

    // format
    vsnprintf(s_logScratchMem, sizeof(s_logScratchMem), fmt, args);

    return s_logScratchMem;
}

void Log::info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    LOCK_HOLDER(s_logSpinLock);

    const char *formattedStr = formatV(fmt, args);

    // clean up varargs
    va_end(args);

    printf("INFO: %s\n", formattedStr);
}

void Log::warn(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    LOCK_HOLDER(s_logSpinLock);

    const char *formattedStr = formatV(fmt, args);

    // clean up varargs
    va_end(args);

    printf("WARNING: %s\n", formattedStr);
}

void Log::err(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    LOCK_HOLDER(s_logSpinLock);

    const char *formattedStr = formatV(fmt, args);

    // clean up varargs
    va_end(args);

    printf("ERROR: %s\n", formattedStr);
}
