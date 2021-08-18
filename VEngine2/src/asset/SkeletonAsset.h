#pragma once
#include "Asset.h"
#include "Handles.h"

/// <summary>
/// AssetData implementation for skinned mesh skeletons.
/// </summary>
class SkeletonAssetData : public AssetData
{
	friend class SkeletonAssetHandler;
public:
	static constexpr AssetType k_assetType = "193A6B40-A4BC-45DA-9BDE-40573D7FE63E"_uuid;

	explicit SkeletonAssetData(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	AnimationSkeletonHandle getSkeletonHandle() const noexcept { return m_skeletonHandle; }

private:
	AnimationSkeletonHandle m_skeletonHandle = {};
};