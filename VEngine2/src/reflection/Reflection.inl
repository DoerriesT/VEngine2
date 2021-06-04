#pragma once
#include "Reflection.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>

template<typename T>
T *Field::getAs(void *instance) noexcept
{
	if (getTypeID<T>() != m_typeID)
	{
		return nullptr;
	}
	return (T *)((uint8_t *)instance + m_offset);
}

template<typename T>
const T *Field::getAs(const void *instance) const noexcept
{
	if (getTypeID<T>() != m_typeID)
	{
		return nullptr;
	}
	return (const T *)((uint8_t *)instance + m_offset);
}

template<typename T>
bool Field::setAs(void *instance, const T &value) noexcept
{
	if (getTypeID<T>() != m_typeID)
	{
		return false;
	}
	T *f = (T *)((uint8_t *)instance + m_offset);
	*f = value;
	return true;
}

template<typename ClassType, typename FieldType>
Reflection::ClassBuilder Reflection::ClassBuilder::addField(const char *name, FieldType ClassType:: *field, const char *displayName, const char *description) noexcept
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

template<typename T>
Reflection::ClassBuilder Reflection::addClass(const char *displayName, const char *description) noexcept
{
	auto type = addType<T>();
	type->m_displayName = displayName;
	type->m_description = description;
	return ClassBuilder(this, type);
}

template<typename T>
Type *Reflection::addType(bool autoreflect) noexcept
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
			c.m_isFloatingPoint = TypeInfo<T>::isFloatingPoint();
			c.m_isSignedInteger = TypeInfo<T>::isSignedInteger();
			c.m_isUnsignedInteger = TypeInfo<T>::isUnsignedInteger();

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

template<typename T>
auto Reflection::reflectType(int) noexcept -> decltype(T::reflect(*this))
{
	T::reflect(*this);
}

template<typename T>
auto Reflection::reflectType(double) noexcept -> void
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



DEFINE_TYPE_INFO(glm::vec2, "B3D6372A-547E-4489-94D0-8FA23D9CA639")

template<>
inline void reflect<glm::vec2>(Reflection &refl) noexcept
{
	refl.addClass<glm::vec2>()
		.addField("x", &glm::vec2::x, "X", "The x-component.")
		.addField("y", &glm::vec2::y, "Y", "The y-component.");
}

DEFINE_TYPE_INFO(glm::vec3, "24BC398F-AA63-4674-BA24-09F222B3C8FD")

template<>
inline void reflect<glm::vec3>(Reflection &refl) noexcept
{
	refl.addClass<glm::vec3>()
		.addField("x", &glm::vec3::x, "X", "The x-component.")
		.addField("y", &glm::vec3::y, "Y", "The y-component.")
		.addField("z", &glm::vec3::z, "Z", "The z-component.");
}

DEFINE_TYPE_INFO(glm::vec4, "A495DCA1-ED06-460D-8249-0F60214C96F4")

template<>
inline void reflect<glm::vec4>(Reflection &refl) noexcept
{
	refl.addClass<glm::vec4>()
		.addField("x", &glm::vec4::x, "X", "The x-component.")
		.addField("y", &glm::vec4::y, "Y", "The y-component.")
		.addField("z", &glm::vec4::z, "Z", "The z-component.")
		.addField("w", &glm::vec4::w, "W", "The w-component.");
}

DEFINE_TYPE_INFO(glm::quat, "E8E783A4-625C-461D-A421-B53C13E4A7F2")

template<>
inline void reflect<glm::quat>(Reflection &refl) noexcept
{
	refl.addClass<glm::quat>()
		.addField("x", &glm::quat::x, "X", "The x-component.")
		.addField("y", &glm::quat::y, "Y", "The y-component.")
		.addField("z", &glm::quat::z, "Z", "The z-component.")
		.addField("w", &glm::quat::w, "W", "The w-component.");
}