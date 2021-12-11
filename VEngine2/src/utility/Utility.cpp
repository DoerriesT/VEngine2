#include "Utility.h"
#include <Windows.h>
#include <fstream>
#include <assert.h>
#include <math.h>

namespace
{
	union FloatUint32Union
	{
		float f;
		uint32_t ui;
	};
}

char *util::readBinaryFile(const char *filepath, size_t *fileSize)
{
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		std::string msg = "Failed to open file " + std::string(filepath) + "!";
		fatalExit(msg.c_str(), -1);
	}

	*fileSize = (size_t)file.tellg();
	file.seekg(0, std::ios::beg);

	char *buffer = new char[*fileSize];
	if (file.read(buffer, *fileSize))
	{
		return buffer;
	}
	else
	{
		std::string msg = "Failed to read file " + std::string(filepath) + "!";
		fatalExit(msg.c_str(), -1);
		return {};
	}
}

void util::fatalExit(const char *message, int exitCode)
{
	MessageBoxA(nullptr, message, nullptr, MB_OK | MB_ICONERROR);
	exit(exitCode);
}

eastl::string util::getFileExtension(const eastl::string &filepath)
{
	return filepath.substr(filepath.find_last_of('.') + 1);
}

uint32_t util::findFirstSetBit(uint32_t mask)
{
	uint32_t r = 1;

	if (!mask)
	{
		return UINT32_MAX;
	}

	if (!(mask & 0xFFFF))
	{
		mask >>= 16;
		r += 16;
	}
	if (!(mask & 0xFF))
	{
		mask >>= 8;
		r += 8;
	}
	if (!(mask & 0xF))
	{
		mask >>= 4;
		r += 4;
	}
	if (!(mask & 3))
	{
		mask >>= 2;
		r += 2;
	}
	if (!(mask & 1))
	{
		mask >>= 1;
		r += 1;
	}
	return r - 1;
}

uint32_t util::findLastSetBit(uint32_t mask)
{
	uint32_t r = 32;

	if (!mask)
	{
		return UINT32_MAX;
	}

	if (!(mask & 0xFFFF0000u))
	{
		mask <<= 16;
		r -= 16;
	}
	if (!(mask & 0xFF000000u))
	{
		mask <<= 8;
		r -= 8;
	}
	if (!(mask & 0xF0000000u))
	{
		mask <<= 4;
		r -= 4;
	}
	if (!(mask & 0xC0000000u))
	{
		mask <<= 2;
		r -= 2;
	}
	if (!(mask & 0x80000000u))
	{
		mask <<= 1;
		r -= 1;
	}
	return r - 1;
}

bool util::isPowerOfTwo(uint32_t value)
{
	return (value & (value - 1)) == 0 && value > 0;
}

uint32_t util::nextPowerOfTwo(uint32_t value)
{
	assert(value > 0);
	// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;
	return value;
}

float util::halton(size_t index, size_t base)
{
	float f = 1.0f;
	float r = 0.0f;

	while (index > 0)
	{
		f /= base;
		r += f * (index % base);
		index /=base;
	}

	return r;
}

void util::findPieceWiseLinearCurveIndicesAndAlpha(size_t count, const float *keys, float lookupKey, bool wrap, size_t *index0, size_t *index1, float *alpha) noexcept
{
	// simple case with a single sample
	if (count == 1)
	{
		*index0 = 0;
		*index1 = 0;
		*alpha = 0.0f;
		return;
	}
	// lookupKey is less than first key
	else if (lookupKey < keys[0])
	{
		if (wrap)
		{
			*index0 = count - 1;
			*index1 = 0;
			*alpha = lookupKey / keys[0];
		}
		else
		{
			*index0 = 0;
			*index1 = 0;
			*alpha = 0.0f;
		}
		return;
	}
	// lookupKey is greater than last key
	else if (lookupKey >= keys[count - 1])
	{
		if (wrap)
		{
			float wrappedKey = lookupKey - keys[count - 1];
			*index0 = count - 1;
			*index1 = 0;
			*alpha = keys[0] > 1e-5f ? wrappedKey / keys[0] : 0.0f;
		}
		else
		{
			*index0 = count - 1;
			*index1 = count - 1;
			*alpha = 0.0f;
		}
		return;
	}
	// lookupKey is somewhere between first and last sample
	else
	{
		// search 
		size_t keyIdx0 = 0;
		for (size_t i = 0; i < count; ++i)
		{
			if ((i + 1) == count || lookupKey < keys[i + 1])
			{
				keyIdx0 = i;
				break;
			}
		}

		assert(count > 1);
		assert((keyIdx0 + 1) < count);
		size_t keyIdx1 = keyIdx0 + 1;

		const float key0 = keys[keyIdx0];
		const float key1 = keys[keyIdx1];
		const float diff = key1 - key0;
		*alpha = diff > 1e-5f ? (lookupKey - key0) / diff : 0.0f;
		*index0 = keyIdx0;
		*index1 = keyIdx1;
	}
}

float util::asfloat(uint32_t v) noexcept
{
	FloatUint32Union fu;
	fu.ui = v;
	return fu.f;
}

uint32_t util::asuint(float v) noexcept
{
	FloatUint32Union fu;
	fu.f = v;
	return fu.ui;
}
