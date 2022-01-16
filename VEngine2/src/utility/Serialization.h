#pragma once
#include <stdint.h>
#include <EASTL/vector.h>

class SerializationWriteStream
{
public:
	static constexpr bool isWriting() noexcept { return true; }
	static constexpr bool isReading() noexcept { return false; }

	const eastl::vector<char> &getData() const noexcept { return m_data; }

	bool serialize(bool value) noexcept;
	bool serialize(int32_t value) noexcept;
	bool serialize(uint32_t value) noexcept;
	bool serialize(float value) noexcept;
	bool serialize(size_t length, const char *value) noexcept;

private:
	eastl::vector<char> m_data;
};

class SerializationReadStream
{
public:
	static constexpr bool isWriting() noexcept { return false; }
	static constexpr bool isReading() noexcept { return true; }

	explicit SerializationReadStream(size_t bufferSize, const char *data) noexcept : m_data(data), m_bufferSize(bufferSize) {}

	bool serialize(bool &value) noexcept;
	bool serialize(int32_t &value) noexcept;
	bool serialize(uint32_t &value) noexcept;
	bool serialize(float &value) noexcept;
	bool serialize(size_t length, char *value) noexcept;

private:
	const char *m_data = nullptr;
	size_t m_bufferSize = 0;
	size_t m_readOffset = 0;
};

#define serializeBool(stream, value)															\
	do																							\
	{																							\
		if (!stream.serialize(value))															\
		{																						\
			return false;																		\
		}																						\
	} while (false)

#define serializeInt32(stream, value)															\
	do																							\
	{																							\
		if (!stream.serialize(value))															\
		{																						\
			return false;																		\
		}																						\
	} while (false)

#define serializeUInt32(stream, value)															\
	do																							\
	{																							\
		if (!stream.serialize(value))															\
		{																						\
			return false;																		\
		}																						\
	} while (false)

#define serializeFloat(stream, value)															\
	do																							\
	{																							\
		if (!stream.serialize(value))															\
		{																						\
			return false;																		\
		}																						\
	} while (false)

#define serializeBytes(stream, length, values)													\
	do																							\
	{																							\
		if (!stream.serialize(length, values))													\
		{																						\
			return false;																		\
		}																						\
	} while (false)

#define serializeAsset(stream, asset, AssetDataType)											\
	do																							\
	{																							\
		if constexpr (stream.isWriting())														\
		{																						\
			uint32_t strLen = asset ? (uint32_t)(strlen(asset->getAssetID().m_string) + 1) : 0;	\
			serializeUInt32(stream, strLen);													\
			if (strLen > 0)																		\
			{																					\
				serializeBytes(stream, strLen, asset->getAssetID().m_string);					\
			}																					\
		}																						\
		else if constexpr (stream.isReading())													\
		{																						\
			uint32_t strLen = 0;																\
			serializeUInt32(stream, strLen);													\
			if (strLen > 0)																		\
			{																					\
				char *str = (char *)ALLOC_A(strLen);											\
				serializeBytes(stream, strLen, str);											\
				asset = AssetManager::get()->getAsset<AssetDataType>(AssetID(str));				\
			}																					\
		}																						\
	} while (false)