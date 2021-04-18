#include "Asset.h"
#include "AssetManager.h"

AssetData::AssetData(const AssetID &assetID, const AssetType &assetType) noexcept
	:m_assetID(assetID),
	m_assetType(assetType)
{
}

void AssetData::acquire() noexcept
{
	++m_referenceCount;
}

void AssetData::release() noexcept
{
	assert(m_referenceCount > 0);

	if (m_referenceCount.sub_fetch(1) == 0)
	{
		AssetManager::get()->unloadAsset(m_assetID, getAssetType(), this);
	}
}

int32_t AssetData::getReferenceCount() const noexcept
{
	return m_referenceCount;
}

AssetStatus AssetData::getAssetStatus() const noexcept
{
	return static_cast<AssetStatus>(m_assetStatus.load());
}

const TUUID &AssetData::getAssetID() const noexcept
{
	return m_assetID;
}

const AssetType &AssetData::getAssetType() const noexcept
{
	return m_assetType;
}

void AssetData::setAssetStatus(AssetStatus status) noexcept
{
	m_assetStatus = static_cast<uint32_t>(status);
}
