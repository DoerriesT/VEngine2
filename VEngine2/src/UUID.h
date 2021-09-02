#pragma once
#include <stdint.h>

/// <summary>
/// Represents a universally unique identifier (UUID).
/// </summary>
struct TUUID
{
	static constexpr size_t k_uuidStringSize = 37; // includes space for null-terminator

	union
	{
		// UUID data in byte representation
		uint8_t m_data[16];
		// UUID data in uint64 representation
		uint64_t m_data64[2];
	};

	/// <summary>
	/// Constructs a new invalid null UUID.
	/// </summary>
	/// <returns></returns>
	explicit constexpr TUUID() noexcept = default;

	/// <summary>
	/// Constructs a new UUID from a c-string. The string must represent a UUID and have a length of 36 (+ 1 for the null-terminator).
	/// </summary>
	/// <param name="str">A string representing a UUID. Must be of length 36 (+ 1 for the null-terminator).</param>
	/// <returns></returns>
	explicit inline constexpr TUUID(const char *str) noexcept;

	/// <summary>
	/// Constructs a new UUID from a byte array. The array must have a length of 16.
	/// </summary>
	/// <param name="data">A byte array representing a UUID. Must be of length 16.</param>
	/// <returns></returns>
	explicit inline constexpr TUUID(const uint8_t *data) noexcept;

	/// <summary>
	/// Creates a new randomly generated UUID.
	/// </summary>
	/// <returns>A new randomly generated UUID instance.</returns>
	static TUUID createRandom() noexcept;

	/// <summary>
	/// Combines two UUIDs to create a new one.
	/// </summary>
	/// <param name="lhs">The first UUID to combine.</param>
	/// <param name="rhs">The second UUID to combine.</param>
	/// <returns>The result of combining both UUIDs.</returns>
	inline static constexpr TUUID combine(const TUUID &lhs, const TUUID &rhs) noexcept;

	/// <summary>
	/// Converts UUID to string representation.
	/// </summary>
	/// <param name="str">[In/Out] Pointer to a char array with it least 37 elements. Will contain the resulting null-terminated string</param>
	inline constexpr void toString(char *str) const noexcept;

	/// <summary>
	/// Tests if the UUID is valid (not null).
	/// </summary>
	/// <returns>True if any byte in the UUID is not 0.</returns>
	inline constexpr bool isValid() const noexcept;

	bool operator==(const TUUID &uuid) const noexcept;
	bool operator!=(const TUUID &uuid) const noexcept;

private:
	inline static constexpr bool isValidHex(char c) noexcept;
	inline static constexpr char toLower(char c) noexcept;
	inline static constexpr uint8_t hexToNumber(char c) noexcept;
	inline static constexpr char numberToHex(uint8_t n) noexcept;
	inline static constexpr uint8_t processByte(char c0, char c1, bool &valid) noexcept;
	inline static constexpr void setVariant(uint8_t *data) noexcept;
};

/// <summary>
/// Creates a UUID from a UUID string literal.
/// </summary>
/// <param name="str">A string representing a UUID.</param>
/// <param name="length">The length of the string. Must be 37.</param>
/// <returns></returns>
constexpr TUUID operator ""_uuid(const char *str, size_t length) noexcept;

struct UUIDHash { size_t operator()(const TUUID &value) const noexcept; };



// IMPLEMENTATION

constexpr bool TUUID::isValidHex(char c) noexcept
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
};

constexpr char TUUID::toLower(char c) noexcept
{
	if (c >= 'A' && c <= 'F')
	{
		c += ('a' - 'A');
	}

	return c;
};

constexpr uint8_t TUUID::hexToNumber(char c) noexcept
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

constexpr char TUUID::numberToHex(uint8_t n) noexcept
{
	n &= 0xF; // mask lower 4 bits
	return n < 10 ? ('0' + n) : ('a' + (n - 10));
}

constexpr uint8_t TUUID::processByte(char c0, char c1, bool &valid) noexcept
{
	if (!isValidHex(c0) || !isValidHex(c1))
	{
		valid &= false;
		return 0;
	}

	c0 = toLower(c0);
	c1 = toLower(c1);

	return (hexToNumber(c0) << 4) | hexToNumber(c1);
}

inline constexpr void TUUID::setVariant(uint8_t *data) noexcept
{
	data[6] &= 0x0F; // clear version
	data[6] |= 0x40; // set to version 4
	data[7] &= 0x3F; // clear variant
	data[7] |= 0x80; // set to IETF variant
}

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
		m_data64[0] = 0;
		m_data64[1] = 0;
		// memset(m_data, 0, 16); // not constexpr
	}
	else
	{
		setVariant(m_data);
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

	setVariant(m_data);
}

inline constexpr TUUID TUUID::combine(const TUUID &lhs, const TUUID &rhs) noexcept
{
	union
	{
		uint64_t data[2] = {};
		uint8_t data8[16];
	} u;
	
	u.data[0] = lhs.m_data64[0] ^ (rhs.m_data64[0] + 0x9e3779b9 + (lhs.m_data64[0] << 6) + (lhs.m_data64[0] >> 2));
	u.data[1] = lhs.m_data64[1] ^ (rhs.m_data64[1] + 0x9e3779b9 + (lhs.m_data64[1] << 6) + (lhs.m_data64[1] >> 2));

	return TUUID(u.data8);
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

constexpr TUUID operator""_uuid(const char *str, size_t length) noexcept
{
	return TUUID(str);
}