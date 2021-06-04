#include "Reflection.h"

Reflection g_Reflection;

Reflection::ClassBuilder::ClassBuilder(Reflection *refl, Type *c) noexcept
	:m_refl(refl),
	m_class(c)
{
}

Reflection::ClassBuilder Reflection::ClassBuilder::addAttribute(const Attribute &attribute) noexcept
{
	// this function works on the last added field, so m_fields must not be empty
	assert(!m_class->m_fields.empty());

	// guard against adding the same attribute twice
	AttributeValue dummyAttVal;
	assert(!m_class->m_fields.back().findAttribute(attribute.m_type, &dummyAttVal));

	m_class->m_fields.back().m_attributes.push_back(attribute);

	return *this;
}

const Type *Reflection::getType(const TypeID &typeID) const noexcept
{
	auto it = m_types.find(typeID);
	return it != m_types.end() ? it->second : nullptr;
}

bool Field::getAsFloatingPoint(const void *instance, double *value) noexcept
{
	const uint8_t *valueMem = ((const uint8_t *)instance + m_offset);

	if (getTypeID<float>() == m_typeID)
	{
		*value = (double)(*(const float *)valueMem);
		return true;
	}
	else if (getTypeID<double>() == m_typeID)
	{
		*value = (double)(*(const double *)valueMem);
		return true;
	}
	return false;
}

bool Field::getAsSignedInteger(const void *instance, int64_t *value) noexcept
{
	const uint8_t *valueMem = ((const uint8_t *)instance + m_offset);

	if (getTypeID<int8_t>() == m_typeID)
	{
		*value = (int64_t)(*(const int8_t *)valueMem);
		return true;
	}
	else if (getTypeID<int16_t>() == m_typeID)
	{
		*value = (int64_t)(*(const int16_t *)valueMem);
		return true;
	}
	else if (getTypeID<int32_t>() == m_typeID)
	{
		*value = (int64_t)(*(const int32_t *)valueMem);
		return true;
	}
	else if (getTypeID<int64_t>() == m_typeID)
	{
		*value = (int64_t)(*(const int64_t *)valueMem);
		return true;
	}
	return false;
}

bool Field::getAsUnsignedInteger(const void *instance, uint64_t *value) noexcept
{
	const uint8_t *valueMem = ((const uint8_t *)instance + m_offset);

	if (getTypeID<uint8_t>() == m_typeID)
	{
		*value = (uint64_t)(*(const uint8_t *)valueMem);
		return true;
	}
	else if (getTypeID<uint16_t>() == m_typeID)
	{
		*value = (uint64_t)(*(const uint16_t *)valueMem);
		return true;
	}
	else if (getTypeID<uint32_t>() == m_typeID)
	{
		*value = (uint64_t)(*(const uint32_t *)valueMem);
		return true;
	}
	else if (getTypeID<uint64_t>() == m_typeID)
	{
		*value = (uint64_t)(*(const uint64_t *)valueMem);
		return true;
	}
	return false;
}

bool Field::setAsFloatingPoint(void *instance, double value) noexcept
{
	uint8_t *valueMem = ((uint8_t *)instance + m_offset);

	if (getTypeID<float>() == m_typeID)
	{
		*(float *)valueMem = (float)value;
		return true;
	}
	else if (getTypeID<double>() == m_typeID)
	{
		*(double *)valueMem = (double)value;
		return true;
	}
	return false;
}

bool Field::setAsSignedInteger(void *instance, int64_t value) noexcept
{
	uint8_t *valueMem = ((uint8_t *)instance + m_offset);

	if (getTypeID<int8_t>() == m_typeID)
	{
		*(int8_t *)valueMem = (int8_t)value;
		return true;
	}
	else if (getTypeID<int16_t>() == m_typeID)
	{
		*(int16_t *)valueMem = (int16_t)value;
		return true;
	}
	else if (getTypeID<int32_t>() == m_typeID)
	{
		*(int32_t *)valueMem = (int32_t)value;
		return true;
	}
	else if (getTypeID<int64_t>() == m_typeID)
	{
		*(int64_t *)valueMem = (int64_t)value;
		return true;
	}
	return false;
}

bool Field::setAsUnsignedInteger(void *instance, uint64_t value) noexcept
{
	uint8_t *valueMem = ((uint8_t *)instance + m_offset);

	if (getTypeID<uint8_t>() == m_typeID)
	{
		*(uint8_t *)valueMem = (uint8_t)value;
		return true;
	}
	else if (getTypeID<uint16_t>() == m_typeID)
	{
		*(uint16_t *)valueMem = (uint16_t)value;
		return true;
	}
	else if (getTypeID<uint32_t>() == m_typeID)
	{
		*(uint32_t *)valueMem = (uint32_t)value;
		return true;
	}
	else if (getTypeID<uint64_t>() == m_typeID)
	{
		*(uint64_t *)valueMem = (uint64_t)value;
		return true;
	}
	return false;
}

bool Field::findAttribute(AttributeType type, AttributeValue *value) noexcept
{
	for (const auto &a : m_attributes)
	{
		if (a.m_type == type)
		{
			*value = a.m_value;
			return true;
		}
	}
	return false;
}

Attribute::Attribute(AttributeType type, uint32_t value) noexcept
	:m_type(type)
{
	m_value.m_uint32 = value;
}

Attribute::Attribute(AttributeType type, int32_t value) noexcept
	:m_type(type)
{
	m_value.m_int32 = value;
}

Attribute::Attribute(AttributeType type, float value) noexcept
	:m_type(type)
{
	m_value.m_float = value;
}

Attribute::Attribute(AttributeType type, AttributeGetterFunc value) noexcept
	:m_type(type)
{
	m_value.m_getter = value;
}

Attribute::Attribute(AttributeType type, AttributeSetterFunc value) noexcept
	:m_type(type)
{
	m_value.m_setter = value;
}

Attribute::Attribute(AttributeType type, AttributeUIElements value) noexcept
	:m_type(type)
{
	m_value.m_uiElement = value;
}
