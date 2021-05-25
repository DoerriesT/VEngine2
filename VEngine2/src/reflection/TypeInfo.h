#pragma once
#include "UUID.h"
#include <assert.h>
#include <EASTL/type_traits.h>

using TypeID = TUUID;

static constexpr TypeID k_invalidTypeID = TypeID();
static constexpr TypeID k_ptrTypeID("F44129E0-CF54-4D82-9960-17EAB67901CC");
static constexpr TypeID k_arrayTypeID("FB005B9C-30E3-4ABF-A0DE-278775F03190");

enum TypeCategory
{
	TYPE_CATEGORY_FUNDAMENTAL = 0,
	TYPE_CATEGORY_POINTER = 1,
	TYPE_CATEGORY_CLASS = 2,
	TYPE_CATEGORY_ARRAY = 3
};

/// <summary>
/// TypeInfo struct has static functions to query information about the templated type.
/// Must be specialized for each type.
/// </summary>
/// <typeparam name="T"></typeparam>
template<typename T, size_t ArraySize = 0>
struct TypeInfo
{
	static T defaultConstruct() noexcept
	{
		assert(false);
		T t;
		return t;
	}

	static const char *getName() noexcept
	{
		assert(false);
		return "";
	}

	static const TypeID &getTypeID() noexcept
	{
		assert(false);
		return k_invalidTypeID;
	}

	static const TypeID &getPointedToTypeID() noexcept
	{
		assert(false);
		return k_invalidTypeID;
	}

	static size_t getSize() noexcept
	{
		assert(false);
		return 0;
	}

	static size_t getAlignment() noexcept
	{
		assert(false);
		return 0;
	}

	static TypeCategory getCategory() noexcept
	{
		assert(false);
		return TYPE_CATEGORY_FUNDAMENTAL;
	}

	static size_t getArraySize() noexcept
	{
		assert(false);
		return 0;
	}

	static bool isIntegral() noexcept
	{
		assert(false);
		return false;
	}

	static bool isSigned() noexcept
	{
		assert(false);
		return false;
	}

	static bool isUnsigned() noexcept
	{
		assert(false);
		return false;
	}
};

// macro for specializing TypeInfo for a given type. id must be a UUID in string representation.
#define DEFINE_TYPE_INFO(TypeName, id)						\
	template<>												\
	struct TypeInfo<TypeName>								\
	{														\
		static TypeName defaultConstruct() noexcept			\
		{													\
			return TypeName();								\
		}													\
															\
		static const char *getName() noexcept				\
		{													\
			return #TypeName;								\
		}													\
															\
		static const TypeID &getTypeID() noexcept			\
		{													\
			static const TypeID s_id(id);					\
			return s_id;									\
		}													\
															\
		static const TypeID &getPointedToTypeID() noexcept	\
		{													\
			return k_invalidTypeID;							\
		}													\
															\
		static size_t getSize() noexcept					\
		{													\
			return sizeof(TypeName);						\
		}													\
															\
		static size_t getAlignment() noexcept				\
		{													\
			return alignof(TypeName);						\
		}													\
															\
		static TypeCategory getCategory() noexcept			\
		{													\
			return 											\
				eastl::is_fundamental<TypeName>::value 		\
				? TYPE_CATEGORY_FUNDAMENTAL 				\
				: TYPE_CATEGORY_CLASS;						\
		}													\
															\
		static size_t getArraySize() noexcept				\
		{													\
			return 0;										\
		}													\
															\
		static bool isIntegral() noexcept					\
		{													\
			return eastl::is_integral<TypeName>::value;		\
		}													\
															\
		static bool isSigned() noexcept						\
		{													\
			return eastl::is_signed<TypeName>::value;		\
		}													\
															\
		static bool isUnsigned() noexcept					\
		{													\
			return eastl::is_unsigned<TypeName>::value;		\
		}													\
	};

/// <summary>
/// TypeInfo specialization for pointer types.
/// </summary>
/// <typeparam name="T">The pointed to type.</typeparam>
template<typename T>
struct TypeInfo<T *>
{
	static const char *getName() noexcept
	{
		static char name[128] = {};

		if (!name[0])
		{
			strcat_s(name, TypeInfo<T>::getName());
			strcat_s(name, "*");
		}

		return name;
	}

	static const TypeID &getTypeID() noexcept
	{
		static TypeID id = TypeID::combine(k_ptrTypeID, TypeInfo<T>::getTypeID());
		return id;
	}

	static const TypeID &getPointedToTypeID() noexcept
	{
		return TypeInfo<T>::getTypeID();
	}

	static size_t getSize() noexcept
	{
		return sizeof(T *);
	}

	static size_t getAlignment() noexcept
	{
		return alignof(T *);
	}

	static TypeCategory getCategory() noexcept
	{
		return TYPE_CATEGORY_POINTER;
	}

	static size_t getArraySize() noexcept
	{
		return 0;
	}

	static bool isIntegral() noexcept
	{
		return false;
	}

	static bool isSigned() noexcept
	{
		return false;
	}

	static bool isUnsigned() noexcept
	{
		return false;
	}
};

/// <summary>
/// TypeInfo specialization for array types.
/// </summary>
/// <typeparam name="T">The array element type.</typeparam>
template<typename T, size_t ArraySize>
struct TypeInfo<T[ArraySize]>
{
	static const char *getName() noexcept
	{
		static char name[128] = {};

		if (!name[0])
		{
			strcat_s(name, TypeInfo<T>::getName());
			strcat_s(name, "[");
			char str[12];
			sprintf_s(str, "%d", (int)ArraySize);
			strcat_s(name, str);
			strcat_s(name, "]");
		}

		return name;
	}

	static const TypeID &getTypeID() noexcept
	{
		static TypeID id = TypeID::combine(k_arrayTypeID, TypeInfo<T>::getTypeID());
		return id;
	}

	static const TypeID &getPointedToTypeID() noexcept
	{
		return TypeInfo<T>::getTypeID();
	}

	static size_t getSize() noexcept
	{
		return sizeof(T[ArraySize]);
	}

	static size_t getAlignment() noexcept
	{
		return alignof(T[ArraySize]);
	}

	static TypeCategory getCategory() noexcept
	{
		return TYPE_CATEGORY_ARRAY;
	}

	static size_t getArraySize() noexcept
	{
		return ArraySize;
	}

	static bool isIntegral() noexcept
	{
		return false;
	}

	static bool isSigned() noexcept
	{
		return false;
	}

	static bool isUnsigned() noexcept
	{
		return false;
	}
};

DEFINE_TYPE_INFO(short int, "C49F335A-E62B-4ACB-BC70-45B2F13920BD")
DEFINE_TYPE_INFO(unsigned short int, "1D39718E-6BC5-47B8-980F-D71231DEB34C")
DEFINE_TYPE_INFO(int, "4807FF4D-52BB-4F24-ACD3-1CE2C5AAB84F")
DEFINE_TYPE_INFO(unsigned int, "FB0B092F-04BA-44CA-90AC-45CDF69A6B96")
DEFINE_TYPE_INFO(long int, "2D936AA8-B117-4A26-ADD8-5D538ABE1451")
DEFINE_TYPE_INFO(unsigned long int, "A86249D0-333A-4F50-A703-6696FB10A61C")
DEFINE_TYPE_INFO(long long int, "433126DB-38C4-4D94-B602-D6104AA1B6BF")
DEFINE_TYPE_INFO(unsigned long long int, "776ED030-BB30-40FF-9DAD-2D74D6B0A86C")
DEFINE_TYPE_INFO(bool, "466575B9-6F31-410E-86A8-0DEC1330523D")
DEFINE_TYPE_INFO(signed char, "B4F0227E-13A5-4034-B394-2AB3B231AF73")
DEFINE_TYPE_INFO(unsigned char, "3BCD6322-AA5D-4802-9A15-D5732AB670E6")
DEFINE_TYPE_INFO(char, "5F5D0094-90A7-4EE8-95FC-53C4A633D5CD")
DEFINE_TYPE_INFO(wchar_t, "1227ED7C-E855-4F5A-A005-3A329B38808A")
DEFINE_TYPE_INFO(char16_t, "E03C4D93-8D0C-408E-8EA3-10861E58BA43")
DEFINE_TYPE_INFO(char32_t, "1112F298-E606-4243-8E8F-F92864B2E4F8")
DEFINE_TYPE_INFO(float, "A60A18C4-94E5-42C9-A84C-A5F99184F43E")
DEFINE_TYPE_INFO(double, "35CE5482-9460-400D-A625-1DEAE95D8837")
DEFINE_TYPE_INFO(long double, "2B937A73-06A5-4CC7-A175-38174EB0AD5B")

template<typename T>
const TypeID &getTypeID() noexcept
{
	return TypeInfo<T>::getTypeID();
}
