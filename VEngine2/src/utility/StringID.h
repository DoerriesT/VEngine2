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

inline uint64_t stringHashFNV1a(const char *str) noexcept
{
	constexpr uint64_t fnvPrime = 1099511628211;
	constexpr uint64_t fnvOffsetBasis = 14695981039346656037;

	uint64_t hash = fnvOffsetBasis;
	while (*str)
	{
		hash = (hash ^ *str) * fnvPrime;
		++str;
	}
	return hash;
}

constexpr uint64_t operator""_hash(const char *str, size_t length) noexcept
{
	constexpr uint64_t fnvPrime = 1099511628211;
	constexpr uint64_t fnvOffsetBasis = 14695981039346656037;
	uint64_t hash = length == 1 ? fnvOffsetBasis : operator""_hash(str + 1, length - 1);
	return (hash ^ str[0]) * fnvPrime;
}