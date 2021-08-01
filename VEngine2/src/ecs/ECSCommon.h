#pragma once
#include <stdint.h>
#include <EASTL/bitset.h>

constexpr size_t k_ecsMaxComponentTypes = 64;
constexpr size_t k_ecsComponentsPerMemoryChunk = 1024;

using IDType = uint64_t;
using EntityID = IDType;
using ComponentID = IDType;
using ComponentMask = eastl::bitset<k_ecsMaxComponentTypes>;

constexpr EntityID k_nullEntity = 0;