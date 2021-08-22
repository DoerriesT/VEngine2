#pragma once
#include <stdint.h>

class Path
{
public:
	static bool hasValidForm(const char *path) noexcept;
	static const char *getExtension(const char *path) noexcept;
	static size_t getSegmentCount(const char *path, char separator = '/') noexcept;
	static size_t getFirstNSegments(const char *path, size_t segmentCount, char separator = '/') noexcept;
	static bool getSegmentDelimiters(const char *path, size_t segmentIndex, size_t *start, size_t *end, char separator = '/') noexcept;
};