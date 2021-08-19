#pragma once
#include <stdint.h>

#define SID(str) StringID(str, str##_hash)

struct StringID
{
	uint64_t m_hash = 0;
	const char *m_string = nullptr;

	explicit StringID() noexcept = default;
	explicit StringID(const char *string) noexcept;
	explicit StringID(const char *string, uint64_t hash) noexcept;

	bool operator==(const StringID &sid) const noexcept
	{
		return m_hash == sid.m_hash;
	}

	bool operator!=(const StringID &sid) const noexcept
	{
		return m_hash != sid.m_hash;
	}
};

struct StringIDHash 
{ 
	size_t operator()(const StringID &value) const noexcept
	{
		return (size_t)value.m_string;
	}
};

constexpr uint64_t k_fnvPrime = 1099511628211;
constexpr uint64_t k_fnvOffsetBasis = 14695981039346656037;

inline uint64_t stringHashFNV1a(const char *str) noexcept
{
	uint64_t hash = k_fnvOffsetBasis;
	while (*str)
	{
		hash = (hash ^ *str) * k_fnvPrime;
		++str;
	}
	return hash;
}

constexpr uint64_t stringHashFNV1aRecursive(const char *str, size_t length, uint64_t hash) noexcept
{
	if (*str)
	{
		hash = (hash ^ *str) * k_fnvPrime;
		return stringHashFNV1aRecursive(str + 1, length - 1, hash);
	}
	return hash;
}

constexpr uint64_t operator""_hash(const char *str, size_t length) noexcept
{
	return stringHashFNV1aRecursive(str, length, k_fnvOffsetBasis);
}