#pragma once
#include "Asset.h"
#include "animation/AnimationGraph.h"

/// <summary>
/// AssetData implementation for animation graphs.
/// </summary>
class AnimationGraphAsset : public AssetData
{
	friend class AnimationGraphAssetHandler;
public:
	static constexpr AssetType k_assetType = "3E249858-B91D-4351-9DB5-6F8C7FD32F6D"_uuid;

	enum class Version : uint32_t
	{
		V_1_0 = 0,
		LATEST = V_1_0,
	};

	struct FileHeader
	{
		char m_magicNumber[8] = { 'V', 'E', 'A', 'N', 'I', 'M', 'G', ' ' };
		Version m_version = Version::V_1_0;
		uint32_t m_fileSize;
		uint32_t m_rootNodeIndex;
		uint32_t m_nodeCount;
		uint32_t m_parameterCount;
		uint32_t m_animationClipAssetCount;
	};

	explicit AnimationGraphAsset(const AssetID &assetID) noexcept : AssetData(assetID, k_assetType) {}
	const AnimationGraph *getAnimationGraph() const noexcept { return &m_animationGraph; }
	void setAnimationGraph(const AnimationGraphCreateInfo &graphCreateInfo) noexcept { m_animationGraph = AnimationGraph(graphCreateInfo); }

private:
	AnimationGraph m_animationGraph;
};