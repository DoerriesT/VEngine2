#include "WideNarrowStringConversion.h"
#include <assert.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

eastl::string narrow(const wchar_t *s) noexcept
{
	int requiredSize = ::WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
	
	assert(requiredSize > 0);

	eastl::string result;
	result.resize(requiredSize);
	int writtenSize = ::WideCharToMultiByte(CP_UTF8, 0, s, -1, result.data(), requiredSize, nullptr, nullptr);

	assert(writtenSize == requiredSize);

	return result;
}

eastl::wstring widen(const char *s) noexcept
{
	int requiredSize = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);

	assert(requiredSize > 0);

	eastl::wstring result;
	result.resize(requiredSize);
	int writtenSize = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, result.data(), requiredSize);

	assert(writtenSize == requiredSize);

	return result;
}

bool narrow(const wchar_t *s, size_t resultBufferSize, char *resultBuffer) noexcept
{
	if (!s || !resultBuffer)
	{
		return false;
	}

	int requiredSize = ::WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);

	assert(requiredSize > 0);

	if (requiredSize > resultBufferSize)
	{
		return false;
	}

	int writtenSize = ::WideCharToMultiByte(CP_UTF8, 0, s, -1, resultBuffer, requiredSize, nullptr, nullptr);

	assert(writtenSize == requiredSize);

	return true;
}

bool widen(const char *s, size_t resultBufferSize, wchar_t *resultBuffer) noexcept
{
	if (!s || !resultBuffer)
	{
		return false;
	}

	int requiredSize = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);

	assert(requiredSize > 0);

	if (requiredSize > resultBufferSize)
	{
		return false;
	}

	int writtenSize = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, resultBuffer, requiredSize);

	assert(writtenSize == requiredSize);

	return true;
}
