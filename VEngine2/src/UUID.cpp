#include "UUID.h"
#include <random>
#include <chrono>
#include "Utility/Utility.h"

static std::default_random_engine s_rndEngine((unsigned int)(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
static std::uniform_int_distribution<uint64_t> s_uniformIntDistrib;

TUUID TUUID::createRandom() noexcept
{
	TUUID result{};

	uint64_t mostSigBits = s_uniformIntDistrib(s_rndEngine);
	uint64_t leastSigBits = s_uniformIntDistrib(s_rndEngine);

	uint8_t *mostSigBytes = (uint8_t *)&mostSigBits;
	mostSigBytes[6] &= 0x0F; // clear version
	mostSigBytes[6] |= 0x40; // set to version 4

	uint8_t *leastSigBytes = (uint8_t *)&leastSigBits;
	leastSigBytes[0] &= 0x3F; // clear variant
	leastSigBytes[0] |= 0x80; // set to IETF variant

	for (size_t i = 0; i < 8; ++i)
	{
		result.m_data[i] = mostSigBytes[i];
		result.m_data[i + 8] = leastSigBytes[i];
	}

	return result;
}

bool TUUID::operator==(const TUUID &uuid) const noexcept
{
	return m_data64[0] == uuid.m_data64[0] && m_data64[1] == uuid.m_data64[1];
	// return memcmp(m_data, uuid.m_data, 16) == 0;
}

bool TUUID::operator!=(const TUUID &uuid) const noexcept
{
	return m_data64[0] != uuid.m_data64[0] || m_data64[1] != uuid.m_data64[1];
	// return memcmp(m_data, uuid.m_data, 16) != 0;
}

size_t UUIDHash::operator()(const TUUID &value) const noexcept
{
	size_t h = 0;
	util::hashCombine(h, *(const uint64_t *)&value.m_data[0]);
	util::hashCombine(h, *(const uint64_t *)&value.m_data[8]);
	return h;
}