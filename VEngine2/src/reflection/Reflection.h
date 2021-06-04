#pragma once
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include "TypeInfo.h"

class Reflection;

extern Reflection g_Reflection;

enum class AttributeType : uint32_t
{
	MIN,
	MAX,
	STEP,
	GETTER,
	SETTER,
	UI_ELEMENT
};

enum class AttributeUIElements
{
	DEFAULT,
	DRAG,
	SLIDER,
	EULER_ANGLES
};

using AttributeGetterFunc = void (*)(const void *instance, const TypeID &resultTypeID, void *result);
using AttributeSetterFunc = void (*)(void *instance, const TypeID &valueTypeID, void *value);

union AttributeValue
{
	uint32_t m_uint32;
	int32_t m_int32;
	float m_float;
	AttributeGetterFunc m_getter;
	AttributeSetterFunc m_setter;
	AttributeUIElements m_uiElement;
};

struct Attribute
{
	AttributeType m_type;
	AttributeValue m_value;

	Attribute() = default;
	Attribute(AttributeType type, uint32_t value) noexcept;
	Attribute(AttributeType type, int32_t value) noexcept;
	Attribute(AttributeType type, float value) noexcept;
	Attribute(AttributeType type, AttributeGetterFunc value) noexcept;
	Attribute(AttributeType type, AttributeSetterFunc value) noexcept;
	Attribute(AttributeType type, AttributeUIElements value) noexcept;
};

struct Field
{
	const char *m_name;
	const char *m_displayName;
	const char *m_description;
	TypeID m_typeID;
	size_t m_offset;
	eastl::vector<Attribute> m_attributes;

	bool getAsFloatingPoint(const void *instance, double *value) noexcept;
	bool getAsSignedInteger(const void *instance, int64_t *value) noexcept;
	bool getAsUnsignedInteger(const void *instance, uint64_t *value) noexcept;
	bool setAsFloatingPoint(void *instance, double value) noexcept;
	bool setAsSignedInteger(void *instance, int64_t value) noexcept;
	bool setAsUnsignedInteger(void *instance, uint64_t value) noexcept;

	template<typename T>
	inline T *getAs(void *instance) noexcept;

	template<typename T>
	inline const T *getAs(const void *instance) const noexcept;

	template<typename T>
	inline bool setAs(void *instance, const T &value) noexcept;

	/// <summary>
	/// Tries to find an attribute present on this field.
	/// </summary>
	/// <param name="type">The type of the attribute to find.</param>
	/// <param name="value">[Out] On success, the attribute value will be copied into the object pointed to by this pointer.</param>
	/// <returns>True if the attribute was found.</returns>
	bool findAttribute(AttributeType type, AttributeValue *value) noexcept;
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
	bool m_isFloatingPoint;
	bool m_isSignedInteger;
	bool m_isUnsignedInteger;
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
		explicit ClassBuilder(Reflection *refl, Type *c) noexcept;

		template<typename ClassType, typename FieldType>
		inline ClassBuilder addField(const char *name, FieldType ClassType:: *field, const char *displayName = nullptr, const char *description = nullptr) noexcept;
		ClassBuilder addAttribute(const Attribute &attribute) noexcept;

	private:
		Reflection *m_refl;
		Type *m_class;
	};

	template<typename T>
	inline ClassBuilder addClass(const char *displayName = nullptr, const char *description = nullptr) noexcept;

	const Type *getType(const TypeID &typeID) const noexcept;

	template<typename T>
	inline Type *addType(bool autoreflect = false) noexcept;

private:
	eastl::hash_map<TypeID, Type *, UUIDHash> m_types;

	template<typename T>
	inline auto reflectType(int) noexcept -> decltype(T::reflect(*this));

	template<typename T>
	inline auto reflectType(double) noexcept -> void;
};

// IMPLEMENTATION
#include "Reflection.inl"