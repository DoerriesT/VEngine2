#include "Serialization.h"

bool SerializationWriteStream::serialize(bool value) noexcept
{
	m_data.push_back((char)value);
	return true;
}

bool SerializationWriteStream::serialize(int32_t value) noexcept
{
	char bytes[4];
	memcpy(bytes, &value, 4);
	m_data.push_back(bytes[0]);
	m_data.push_back(bytes[1]);
	m_data.push_back(bytes[2]);
	m_data.push_back(bytes[3]);
	return true;
}

bool SerializationWriteStream::serialize(uint32_t value) noexcept
{
	char bytes[4];
	memcpy(bytes, &value, 4);
	m_data.push_back(bytes[0]);
	m_data.push_back(bytes[1]);
	m_data.push_back(bytes[2]);
	m_data.push_back(bytes[3]);
	return true;
}

bool SerializationWriteStream::serialize(float value) noexcept
{
	char bytes[4];
	memcpy(bytes, &value, 4);
	m_data.push_back(bytes[0]);
	m_data.push_back(bytes[1]);
	m_data.push_back(bytes[2]);
	m_data.push_back(bytes[3]);
	return true;
}

bool SerializationWriteStream::serialize(size_t length, const char *value) noexcept
{
	for (size_t i = 0; i < length; ++i)
	{
		m_data.push_back(value[i]);
	}
	return true;
}

bool SerializationReadStream::serialize(bool &value) noexcept
{
	if ((m_readOffset + 1) > m_bufferSize)
	{
		return false;
	}
	value = (bool)m_data[m_readOffset];
	++m_readOffset;
	return true;
}

bool SerializationReadStream::serialize(int32_t &value) noexcept
{
	if ((m_readOffset + 4) > m_bufferSize)
	{
		return false;
	}
	memcpy(&value, m_data + m_readOffset, 4);
	m_readOffset += 4;
	return true;
}

bool SerializationReadStream::serialize(uint32_t &value) noexcept
{
	if ((m_readOffset + 4) > m_bufferSize)
	{
		return false;
	}
	memcpy(&value, m_data + m_readOffset, 4);
	m_readOffset += 4;
	return true;
}

bool SerializationReadStream::serialize(float &value) noexcept
{
	if ((m_readOffset + 4) > m_bufferSize)
	{
		return false;
	}
	memcpy(&value, m_data + m_readOffset, 4);
	m_readOffset += 4;
	return true;
}

bool SerializationReadStream::serialize(size_t length, char *value) noexcept
{
	if ((m_readOffset + length) > m_bufferSize)
	{
		return false;
	}
	memcpy(value, m_data + m_readOffset, length);
	m_readOffset += length;
	return true;
}
