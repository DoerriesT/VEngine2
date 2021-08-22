#include "Path.h"

bool Path::hasValidForm(const char *path) noexcept
{
	return false;
}

const char *Path::getExtension(const char *path) noexcept
{
	int lastPointPosition = -1;

	int curOffset = 0;

	while (path[curOffset])
	{
		if (path[curOffset] == '.')
		{
			lastPointPosition = curOffset;
		}
		++curOffset;
	}

	return lastPointPosition != -1 ? (path + lastPointPosition + 1) : nullptr;
}

const char *Path::getFileName(const char *path, char separator) noexcept
{
	size_t start = 0;
	size_t curOffset = 0;

	while (path[curOffset])
	{
		if (path[curOffset] == separator)
		{
			start = curOffset;
		}
		++curOffset;
	}

	return path + start + 1;
}

size_t Path::getSegmentCount(const char *path, char separator) noexcept
{
	size_t count = 0;
	size_t curOffset = 0;
	// path might not start with a separator, so act as if there is one at position -1 if thats the case
	bool prevWasSeparator = path[0] != separator;

	while (path[curOffset])
	{
		// we are starting a new segment
		if (prevWasSeparator && path[curOffset] != separator)
		{
			++count;
		}

		prevWasSeparator = path[curOffset] == separator;
		++curOffset;
	}

	return count;
}

size_t Path::getFirstNSegments(const char *path, size_t segmentCount, char separator) noexcept
{
	if (segmentCount == 0)
	{
		return 0;
	}
	size_t curOffset = 0;
	size_t count = 0;
	// path might not start with a separator, so act as if there is one at position -1 if thats the case
	bool prevWasSeparator = path[0] != separator;

	while (path[curOffset])
	{
		// we are starting a new segment
		if (prevWasSeparator && path[curOffset] != separator)
		{
			++count;
		}

		if (count == segmentCount && (path[curOffset + 1] == separator || !path[curOffset + 1]))
		{
			++curOffset; // we want curOffset to point at the null terminator or the next separator
			break;
		}

		prevWasSeparator = path[curOffset] == separator;
		++curOffset;
	}

	return curOffset;
}

bool Path::getSegmentDelimiters(const char *path, size_t segmentIndex, size_t *start, size_t *end, char separator) noexcept
{
	*start = 0;
	*end = 0;

	size_t curOffset = 0;
	size_t count = 0;
	// path might not start with a separator, so act as if there is one at position -1 if thats the case
	bool prevWasSeparator = path[0] != separator;

	while (path[curOffset])
	{
		const bool curIsSeparator = path[curOffset] == separator;

		// we are starting a new segment
		if (prevWasSeparator && !curIsSeparator)
		{
			if (count == segmentIndex)
			{
				*start = curOffset;
			}
			++count;
		}

		if (count == (segmentIndex + 1) && (path[curOffset + 1] == separator || !path[curOffset + 1]))
		{
			*end = curOffset + 1;
			break;
		}

		prevWasSeparator = curIsSeparator;
		++curOffset;
	}

	return *start < *end;
}
