#pragma once
#include <EASTL/string.h>
#include <assert.h>

namespace util
{
	char *readBinaryFile(const char *filepath, size_t *fileSize);
	void fatalExit(const char *message, int exitCode);
	eastl::string getFileExtension(const eastl::string &filepath);
	uint32_t findFirstSetBit(uint32_t mask);
	uint32_t findLastSetBit(uint32_t mask);
	bool isPowerOfTwo(uint32_t value);
	uint32_t nextPowerOfTwo(uint32_t value);

	template <typename T>
	inline T alignUp(T value, T alignment)
	{
		return (value + alignment - 1) / alignment * alignment;
	}

	template <typename T>
	inline T alignDown(T value, T alignment)
	{
		return value / alignment * alignment;
	}

	template <typename T>
	inline T alignPow2Up(T value, T alignment)
	{
		const T mask = alignment - 1;
		assert((alignment & mask) == 0);
		return (value + mask) & ~mask;
	}

	template <class T>
	inline void hashCombine(size_t &s, const T &v)
	{
		eastl::hash<T> h;
		s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
	}

	float halton(size_t index, size_t base);

	void findPieceWiseLinearCurveIndicesAndAlpha(size_t count, const float *keys, float lookupKey, bool wrap, size_t *index0, size_t *index1, float *alpha) noexcept;
}