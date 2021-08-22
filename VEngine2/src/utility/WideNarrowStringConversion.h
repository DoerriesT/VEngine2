#pragma once
#include <EASTL/string.h>

eastl::string narrow(const wchar_t *s) noexcept;
eastl::wstring widen(const char *s) noexcept;