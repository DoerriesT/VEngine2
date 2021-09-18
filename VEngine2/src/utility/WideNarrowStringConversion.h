#pragma once
#include <EASTL/string.h>

eastl::string narrow(const wchar_t *s) noexcept;
eastl::wstring widen(const char *s) noexcept;

bool narrow(const wchar_t *s, size_t resultBufferSize, char *resultBuffer) noexcept;
bool widen(const char *s, size_t resultBufferSize, wchar_t *resultBuffer) noexcept;