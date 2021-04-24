#pragma once
#include <EASTL/atomic.h>
#include "UUID.h"
#include <assert.h>
#include "utility/StringID.h"

#ifdef ERROR
#undef ERROR
#endif

typedef TUUID AssetType;
typedef StringID AssetID;

enum class AssetStatus : uint32_t
{
	UNLOADED,
	QUEUED_FOR_LOADING,
	LOADING,
	READY,
	ERROR
};

class AssetData
{
public:
	explicit AssetData(const AssetID &assetID, const AssetType &assetType) noexcept;
	void acquire() noexcept;
	void release() noexcept;
	int32_t getReferenceCount() const noexcept;
	AssetStatus getAssetStatus() const noexcept;
	const AssetID &getAssetID() const noexcept;
	const AssetType &getAssetType() const noexcept;
	void setAssetStatus(AssetStatus status) noexcept;

private:
	eastl::atomic<int32_t> m_referenceCount = 0;
	eastl::atomic<uint32_t> m_assetStatus = static_cast<uint32_t>(AssetStatus::UNLOADED);
	AssetID m_assetID;
	AssetType m_assetType;
};

template<typename T>
class Asset
{
public:
	Asset(AssetData *assetData = nullptr) noexcept;
	template<typename T1>
	Asset(const Asset<T1> &other) noexcept;
	template<typename T1>
	Asset(Asset<T1> &&other) noexcept;
	template<typename T1>
	Asset &operator=(const Asset<T1> &other) noexcept;
	template<typename T1>
	Asset &operator=(Asset<T1> &&other) noexcept;
	~Asset() noexcept;
	bool release() noexcept;
	T *get() const noexcept;
	T &operator*() const noexcept;
	T *operator->() const noexcept;

private:
	AssetData *m_assetData = nullptr;
};

template<typename T>
inline Asset<T>::Asset(AssetData *assetData) noexcept
	:m_assetData(assetData)
{
	if (m_assetData)
	{
		m_assetData->acquire();
	}
}

template<typename T>
template<typename T1>
inline Asset<T>::Asset(const Asset<T1> &other) noexcept
{
	m_assetData = other.get();
	if (m_assetData)
	{
		m_assetData->acquire();
	}
}

template<typename T>
template<typename T1>
inline Asset<T>::Asset(Asset<T1> &&other) noexcept
{
	m_assetData = other.get();
	if (m_assetData)
	{
		m_assetData->acquire();
	}
}

template<typename T>
template<typename T1>
inline Asset<T> &Asset<T>::operator=(const Asset<T1> &other) noexcept
{
	if (&other != this)
	{
		release();

		m_assetData = other.get();
		
		if (m_assetData)
		{
			m_assetData->acquire();
		}
	}
}

template<typename T>
template<typename T1>
inline Asset<T> &Asset<T>::operator=(Asset<T1> &&other) noexcept
{
	if (&other != this)
	{
		release();

		m_assetData = other.get();

		if (m_assetData)
		{
			m_assetData->acquire();
		}
	}
}

template<typename T>
inline Asset<T>::~Asset() noexcept
{
	if (m_assetData)
	{
		m_assetData->release();
	}
}

template<typename T>
inline bool Asset<T>::release() noexcept
{
	if (m_assetData)
	{
		m_assetData->release();
		m_assetData = nullptr;
		return true;
	}
	return false;
}

template<typename T>
inline T *Asset<T>::get() const noexcept
{
	return (T *)m_assetData;
}

template<typename T>
inline T &Asset<T>::operator*() const noexcept
{
	assert(m_assetData);
	return *get();
}

template<typename T>
inline T *Asset<T>::operator->() const noexcept
{
	assert(m_assetData);
	return get();
}
