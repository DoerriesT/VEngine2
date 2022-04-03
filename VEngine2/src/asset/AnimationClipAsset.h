#pragma once
#include "Asset.h"
#include "animation/AnimationClip.h"

/// <summary>
/// AssetData implementation for animation clips.
/// </summary>
class AnimationClipAsset : public AssetData
{
	friend class AnimationClipAssetHandler;
public:
	static constexpr AssetType k_assetType = "7DFD6BE8-A4EF-4BDE-AAB1-2354612ED211"_uuid;

	enum class Version : uint32_t
	{
		V_1_0 = 0,
		LATEST = V_1_0,
	};

	struct FileHeader
	{
		char m_magicNumber[8] = { 'V', 'E', 'A', 'N', 'I', 'M', ' ', ' ' };
		Version m_version = Version::V_1_0;
		uint32_t m_fileSize;
		uint32_t m_jointCount;
		float m_duration;
	};

	explicit AnimationClipAsset(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	const AnimationClip *getAnimationClip() const noexcept { return &m_animationClip; }

private:
	AnimationClip m_animationClip;
};