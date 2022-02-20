#pragma once
#include <stdint.h>
#include <EASTL/bitset.h>

constexpr size_t k_ecsMaxComponentTypes = 64;

using IDType = uint64_t;
using EntityID = uint64_t;
using ComponentID = IDType;
using ComponentMask = eastl::bitset<k_ecsMaxComponentTypes>;

constexpr EntityID k_nullEntity = 0;