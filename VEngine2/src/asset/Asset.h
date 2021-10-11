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
	virtual ~AssetData() noexcept = default;
	void acquire() noexcept;
	void release() noexcept;
	int32_t getReferenceCount() const noexcept;
	AssetStatus getAssetStatus() const noexcept;
	const AssetID &getAssetID() const noexcept;
	const AssetType &getAssetType() const noexcept;
	bool isReloadedAssetAvailable() const noexcept;
	void setAssetStatus(AssetStatus status) noexcept;
	void setIsReloadedAssetAvailable(bool reloadedAssetAvailable) noexcept;

private:
	eastl::atomic<int32_t> m_referenceCount = 0;
	eastl::atomic<uint32_t> m_assetStatus = static_cast<uint32_t>(AssetStatus::UNLOADED);
	eastl::atomic_flag m_reloadedAssetAvailable = false;
	AssetID m_assetID;
	AssetType m_assetType;
};

template<typename T>
class Asset
{
public:
	Asset(AssetData *assetData = nullptr) noexcept;
	Asset(const Asset<T> &other) noexcept;
	Asset(Asset<T> &&other) noexcept;
	Asset &operator=(const Asset<T> &other) noexcept;
	Asset &operator=(Asset<T> &&other) noexcept;
	~Asset() noexcept;
	bool release() noexcept;
	bool isLoaded() const noexcept;
	T *get() const noexcept;
	T &operator*() const noexcept;
	T *operator->() const noexcept;
	operator bool() const noexcept;

private:
	T *m_assetData = nullptr;
};

template<typename T>
inline Asset<T>::Asset(AssetData *assetData) noexcept
	:m_assetData((T *)assetData)
{
	// compile time check to ensure we only use this class with class derived from AssetData
	static_assert(eastl::is_base_of<AssetData, T>::value /* && !eastl::is_same<AssetData, T>::value*/, "Template type T in Asset<T> is not derived from AssetData!");

	// run-time check to ensure that the cast from AssetData to T was valid. Note that this test is not 100% bullet proof.
	//assert(assetData->getAssetType() == T::k_assetType);

	if (m_assetData)
	{
		m_assetData->acquire();
	}
}

template<typename T>
inline Asset<T>::Asset(const Asset<T> &other) noexcept
{
	m_assetData = other.get();
	if (m_assetData)
	{
		m_assetData->acquire();
	}
}

template<typename T>
inline Asset<T>::Asset(Asset<T> &&other) noexcept
{
	m_assetData = other.get();
	if (m_assetData)
	{
		m_assetData->acquire();
	}
}

template<typename T>
inline Asset<T> &Asset<T>::operator=(const Asset<T> &other) noexcept
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

	return *this;
}

template<typename T>
inline Asset<T> &Asset<T>::operator=(Asset<T> &&other) noexcept
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

	return *this;
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
inline bool Asset<T>::isLoaded() const noexcept
{
	return m_assetData && m_assetData->getAssetStatus() == AssetStatus::READY;
}

template<typename T>
inline T *Asset<T>::get() const noexcept
{
	return m_assetData;
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

template<typename T>
inline Asset<T>::operator bool() const noexcept
{
	return m_assetData != nullptr;
}