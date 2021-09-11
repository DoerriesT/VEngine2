#pragma once
#include "Asset.h"
#include "animation/Skeleton.h"

/// <summary>
/// AssetData implementation for skinned mesh skeletons.
/// </summary>
class SkeletonAssetData : public AssetData
{
	friend class SkeletonAssetHandler;
public:
	static constexpr AssetType k_assetType = "193A6B40-A4BC-45DA-9BDE-40573D7FE63E"_uuid;

	enum class Version : uint32_t
	{
		V_1_0 = 0,
		LATEST = V_1_0,
	};

	struct FileHeader
	{
		char m_magicNumber[8] = { 'V', 'E', 'S', 'K', 'E', 'L', ' ', ' ' };
		Version m_version = Version::V_1_0;
		uint32_t m_fileSize;
		uint32_t m_jointCount;
	};

	explicit SkeletonAssetData(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	const Skeleton *getSkeleton() const noexcept { return &m_skeleton; }

private:
	Skeleton m_skeleton;
};