#pragma once
#include <stdint.h>

template <size_t S>
struct ENUM_FLAG_INTEGER_FOR_SIZE;

template <>
struct ENUM_FLAG_INTEGER_FOR_SIZE<1>
{
    typedef int8_t type;
};

template <>
struct ENUM_FLAG_INTEGER_FOR_SIZE<2>
{
    typedef int16_t type;
};

template <>
struct ENUM_FLAG_INTEGER_FOR_SIZE<4>
{
    typedef int32_t type;
};

template <>
struct ENUM_FLAG_INTEGER_FOR_SIZE<8>
{
    typedef int64_t type;
};

// used as an approximation of std::underlying_type<T>
template <class T>
struct ENUM_FLAG_SIZED_INTEGER
{
    typedef typename ENUM_FLAG_INTEGER_FOR_SIZE<sizeof(T)>::type type;
};

#define DEF_ENUM_FLAG_OPERATORS(ENUMTYPE) \
inline constexpr ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) throw() { return ENUMTYPE(((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) | ((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator |= (ENUMTYPE &a, ENUMTYPE b) throw() { return (ENUMTYPE &)(((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) |= ((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline constexpr ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) throw() { return ENUMTYPE(((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) & ((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator &= (ENUMTYPE &a, ENUMTYPE b) throw() { return (ENUMTYPE &)(((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) &= ((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline constexpr ENUMTYPE operator ~ (ENUMTYPE a) throw() { return ENUMTYPE(~((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a)); } \
inline constexpr ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) throw() { return ENUMTYPE(((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) ^ ((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline ENUMTYPE &operator ^= (ENUMTYPE &a, ENUMTYPE b) throw() { return (ENUMTYPE &)(((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type &)a) ^= ((ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
inline constexpr bool operator == (ENUMTYPE a, ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type b) throw() { return (ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a == b; } \
inline constexpr bool operator != (ENUMTYPE a, ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type b) throw() { return (ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a != b; }