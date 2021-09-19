#pragma once
#include <malloc.h>

/// Makes a stack allocation, this allocation gets cleared on function exit
/// NOT on scope exit
#define ALLOC_A(numBytes) alloca(numBytes)

/// Makes a typed stack allocation, this allocation gets cleared on function exit
/// NOT on scope exit
#define ALLOC_A_T(type, count) ((type *)alloca((count) * sizeof(type)))