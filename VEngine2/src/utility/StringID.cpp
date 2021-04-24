#include "StringID.h"
#include <EASTL/hash_map.h>
#include "SpinLock.h"
#include <assert.h>

static eastl::hash_map<uint64_t, const char *> s_stringPool;
static SpinLock s_stringPoolMutex;

static const char *getFromPool(const char *string, uint64_t hash) noexcept
{
	const char **pooledStr = nullptr;
	{
		LOCK_HOLDER(s_stringPoolMutex);
		pooledStr = &s_stringPool[hash];
	}

	// string did not exist before -> allocate memory and store in pool
	if (!(*pooledStr))
	{
		*pooledStr = _strdup(string);
	}
	else
	{
		// we found a hash collision!
		assert(strcmp(*pooledStr, string) == 0);
	}

	return *pooledStr;
}

StringID::StringID(const char *string) noexcept
{
	m_hash = stringHashFNV1a(string);
	m_string = getFromPool(string, m_hash);
}

StringID::StringID(const char *string, uint64_t hash) noexcept
	:m_hash(hash)
{
	m_string = getFromPool(string, m_hash);
}
