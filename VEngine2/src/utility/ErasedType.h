#pragma once

template<typename T>
void erasedTypeDefaultConstruct(void *mem) noexcept
{
	new (mem) T();
}

template<typename T>
void erasedTypeCopyConstruct(void *destination, const void *source) noexcept
{
	new (destination) T(*reinterpret_cast<const T *>(source));
}

template<typename T>
void erasedTypeMoveConstruct(void *destination, void *source) noexcept
{
	new (destination) T(static_cast<T &&>(*reinterpret_cast<T *>(source)));
}

template<typename T>
void erasedTypeCopyAssign(void *destination, const void *source) noexcept
{
	*reinterpret_cast<T *>(destination) = *reinterpret_cast<const T *>(source);
}

template<typename T>
void erasedTypeMoveAssign(void *destination, void *source) noexcept
{
	*reinterpret_cast<T *>(destination) = static_cast<T &&>(*reinterpret_cast<T *>(source));
}

template<typename T>
void erasedTypeDestructor(void *mem) noexcept
{
	reinterpret_cast<T *>(mem)->~T();
}

struct ErasedType
{
	size_t m_size = 0;
	size_t m_alignment = 0;
	void (*m_defaultConstructor)(void *mem) = nullptr;
	void (*m_copyConstructor)(void *destination, const void *source) = nullptr;
	void (*m_moveConstructor)(void *destination, void *source) = nullptr;
	void (*m_copyAssign)(void *destination, const void *source) = nullptr;
	void (*m_moveAssign)(void *destination, void *source) = nullptr;
	void (*m_destructor)(void *mem) = nullptr;

	template<typename T>
	inline static ErasedType create()
	{
		ErasedType info{};
		info.m_size = sizeof(T);
		info.m_alignment = alignof(T);
		info.m_defaultConstructor = erasedTypeDefaultConstruct<T>;
		info.m_copyConstructor = erasedTypeCopyConstruct<T>;
		info.m_moveConstructor = erasedTypeMoveConstruct<T>;
		info.m_copyAssign = erasedTypeCopyAssign<T>;
		info.m_moveAssign = erasedTypeMoveAssign<T>;
		info.m_destructor = erasedTypeDestructor<T>;

		return info;
	}
};