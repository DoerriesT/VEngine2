#include "UUID.h"
#include <random>
#include <chrono>
#include "Utility/Utility.h"

static std::default_random_engine s_rndEngine((unsigned int)(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
static std::uniform_int_distribution<uint64_t> s_uniformIntDistrib;

static constexpr bool isValidHex(char c) noexcept
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
};

static constexpr char toLower(char c) noexcept
{
	if (c >= 'A' && c <= 'F')
	{
		c += ('a' - 'A');
	}

	return c;
};

static constexpr uint8_t hexToNumber(char c) noexcept
{
	if (c >= 'a')
	{
		return 10 + (c - 'a');
	}
	else
	{
		return c - '0';
	}
};

static constexpr char numberToHex(uint8_t n) noexcept
{
	n &= 0xF; // mask lower 4 bits
	return n < 10 ? ('0' + n) : ('a' + (n - 10));
}

static constexpr uint8_t processByte(char c0, char c1, bool &valid) noexcept
{
	if (!isValidHex(c0) || !isValidHex(c1))
	{
		valid &= false;
		return 0;
	}

	c0 = toLower(c0);
	c1 = toLower(c1);

	return (hexToNumber(c0) << 4) | hexToNumber(c1);
};

constexpr TUUID::TUUID(const char *str) noexcept
	:m_data()
{
	//size_t len = strlen(str);
	//if (len != 36)
	//{
	//	return;
	//}

	bool valid = true;

	size_t curStrOffset = 0;
	size_t curDataOffset = 0;

	size_t sizes[] = { 4, 2, 2, 2, 6 };
	for (size_t s = 0; s < 5; ++s)
	{
		for (size_t i = 0; i < sizes[s]; ++i)
		{
			m_data[curDataOffset++] = processByte(str[curStrOffset + 0], str[curStrOffset + 1], valid);
			curStrOffset += 2;
		}
		if (s < 4)
		{
			valid &= str[curStrOffset] == '-';
			++curStrOffset;
		}
	}

	if (!valid)
	{
		memset(m_data, 0, 16);
	}
}

constexpr TUUID::TUUID(const uint8_t *data) noexcept
	:m_data()
{
	for (size_t i = 0; i < 16; ++i)
	{
		m_data[i] = data[i];
	}
	// memcpy(m_data, data, 16); // not constexpr
}

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

constexpr void TUUID::toString(char *str) const noexcept
{
	size_t curDataOffset = 0;
	size_t curStrOffset = 0;
	constexpr size_t sizes[] = { 4, 2, 2, 2, 6 };
	for (size_t s = 0; s < 5; ++s)
	{
		for (size_t i = 0; i < sizes[s]; ++i)
		{
			str[curStrOffset++] = numberToHex(m_data[curDataOffset] >> 4);
			str[curStrOffset++] = numberToHex(m_data[curDataOffset] >> 0);
			++curDataOffset;
		}
		str[curStrOffset++] = '-';
	}

	str[curStrOffset - 1] = '\0';
}

constexpr bool TUUID::isValid() const noexcept
{
	return m_data64[0] != 0 || m_data64[1] != 0;
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

constexpr TUUID operator""_uuid(const char *str, size_t length) noexcept
{
	return TUUID(str);
}