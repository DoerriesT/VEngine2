#pragma once
#include <stdint.h>

/// <summary>
/// Represents a universally unique identifier (UUID).
/// </summary>
struct TUUID
{
	static constexpr size_t s_uuidStringSize = 37; // includes space for null-terminator

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
	explicit constexpr TUUID(const char *str) noexcept;

	/// <summary>
	/// Constructs a new UUID from a byte array. The array must have a length of 16.
	/// </summary>
	/// <param name="data">A byte array representing a UUID. Must be of length 16.</param>
	/// <returns></returns>
	explicit constexpr TUUID(const uint8_t *data) noexcept;

	/// <summary>
	/// Creates a new randomly generated UUID.
	/// </summary>
	/// <returns>A new randomly generated UUID instance.</returns>
	static TUUID createRandom() noexcept;

	/// <summary>
	/// Converts UUID to string representation.
	/// </summary>
	/// <param name="str">[In/Out] Pointer to a char array with it least 37 elements. Will contain the resulting null-terminated string</param>
	constexpr void toString(char *str) const noexcept;

	/// <summary>
	/// Tests if the UUID is valid (not null).
	/// </summary>
	/// <returns>True if any byte in the UUID is not 0.</returns>
	constexpr bool isValid() const noexcept;

	bool operator==(const TUUID &uuid) const noexcept;
	bool operator!=(const TUUID &uuid) const noexcept;
};

/// <summary>
/// Creates a UUID from a UUID string literal.
/// </summary>
/// <param name="str">A string representing a UUID.</param>
/// <param name="length">The length of the string. Must be 37.</param>
/// <returns></returns>
constexpr TUUID operator ""_uuid(const char *str, size_t length) noexcept;

struct UUIDHash { size_t operator()(const TUUID &value) const noexcept; };