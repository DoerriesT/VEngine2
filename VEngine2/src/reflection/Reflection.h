#pragma once
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include "TypeInfo.h"
#include <glm/vec3.hpp>

class Reflection;

extern Reflection g_Reflection;

struct Field
{
	const char *m_name;
	const char *m_displayName;
	const char *m_description;
	TypeID m_typeID;
	size_t m_offset;

	template<typename T>
	T *getAs(void *instance) noexcept
	{
		if (getTypeID<T>() != m_typeID)
		{
			return nullptr;
		}
		return (T *)((uint8_t *)instance + m_offset);
	}

	template<typename T>
	const T *getAs(const void *instance) const noexcept
	{
		if (getTypeID<T>() != m_typeID)
		{
			return nullptr;
		}
		return (const T *)((uint8_t *)instance + m_offset);
	}

	template<typename T>
	bool setAs(void *instance, T &&value)
	{
		if (getTypeID<T>() != m_typeID)
		{
			return false;
		}
		T *f = (T *)((uint8_t *)instance + m_offset);
		*f = value;
		return true;
	}
};

struct Type
{
	TypeID m_typeID;
	const char *m_name;
	const char *m_displayName = nullptr;
	const char *m_description = nullptr;
	size_t m_size;
	size_t m_alignment;
	size_t m_arraySize;
	TypeCategory m_typeCategory;
	TypeID m_pointedToTypeID;
	bool m_isIntegral;
	bool m_isSigned;
	bool m_isUnsigned;
	eastl::vector<Field> m_fields;
};

class Reflection;


template<typename T>
void reflect(Reflection &) noexcept
{
	assert(false);
}

class Reflection
{
public:
	class ClassBuilder
	{
	public:
		explicit ClassBuilder(Reflection *refl, Type *c) noexcept
			:m_refl(refl),
			m_class(c)
		{
		}

		template<typename ClassType, typename FieldType>
		ClassBuilder addField(const char *name, FieldType ClassType:: *field, const char *displayName = nullptr, const char *description = nullptr) noexcept
		{
			m_refl->addType<FieldType>(true);

			Field f{};
			f.m_name = name;
			f.m_displayName = displayName;
			f.m_description = description;
			f.m_typeID = getTypeID<FieldType>();
			f.m_offset = reinterpret_cast<size_t>(&(reinterpret_cast<ClassType const volatile *>(0)->*field));

			m_class->m_fields.push_back(f);

			return *this;
		}

	private:
		Reflection *m_refl;
		Type *m_class;
	};

	template<typename T>
	ClassBuilder addClass(const char *displayName = nullptr, const char *description = nullptr) noexcept
	{
		auto type = addType<T>();
		type->m_displayName = displayName;
		type->m_description = description;
		return ClassBuilder(this, type);
	}

	const Type *getType(const TypeID &typeID) const noexcept
	{
		auto it = m_types.find(typeID);
		return it != m_types.end() ? it->second : nullptr;
	}

	template<typename T>
	Type *addType(bool autoreflect = false) noexcept
	{
		if (autoreflect)
		{
			reflectType<T>(0);
			assert(m_types.find(getTypeID<T>()) != m_types.end());
			return m_types[getTypeID<T>()];
		}
		else
		{
			Type *ptr = nullptr;
			auto it = m_types.find(getTypeID<T>());
			if (it == m_types.end())
			{
				Type c;
				c.m_typeID = getTypeID<T>();
				c.m_name = TypeInfo<T>::getName();
				c.m_size = TypeInfo<T>::getSize();
				c.m_alignment = TypeInfo<T>::getAlignment();
				c.m_arraySize = TypeInfo<T>::getArraySize();
				c.m_typeCategory = TypeInfo<T>::getCategory();
				c.m_pointedToTypeID = TypeInfo<T>::getPointedToTypeID();
				c.m_isIntegral = TypeInfo<T>::isIntegral();
				c.m_isSigned = TypeInfo<T>::isSigned();
				c.m_isUnsigned = TypeInfo<T>::isUnsigned();

				ptr = new Type(c);
				m_types[c.m_typeID] = ptr;

				if (eastl::is_pointer<T>::value)
				{
					addType<eastl::remove_pointer<T>::type>(true);
				}
				else if (eastl::is_array<T>::value)
				{
					addType<eastl::remove_extent<T>::type>(true);
				}

				return ptr;
			}
			else
			{
				return it->second;
			}
		}
	}

private:
	eastl::hash_map<TypeID, Type *, UUIDHash> m_types;

	template<typename T>
	auto reflectType(int) noexcept -> decltype(T::reflect(*this))
	{
		T::reflect(*this);
	}

	template<typename T>
	auto reflectType(double) noexcept -> void
	{
		if (eastl::is_class<T>::value)
		{
			reflect<T>(*this);
		}
		else
		{
			addType<T>(false);
		}
	}
};

DEFINE_TYPE_INFO(glm::vec3, "24BC398F-AA63-4674-BA24-09F222B3C8FD")

template<>
inline void reflect<glm::vec3>(Reflection &refl) noexcept
{
	refl.addClass<glm::vec3>()
		.addField("x", &glm::vec3::x, "X", "The x-component.")
		.addField("y", &glm::vec3::y, "Y", "The y-component.")
		.addField("z", &glm::vec3::z, "Z", "The z-component.");
}