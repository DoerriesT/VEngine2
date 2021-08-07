#pragma once
#include <animation/AnimationGraph.h>
#include <asset/AnimationClipAsset.h>


class CustomAnimGraph : public AnimationGraph
{
	friend class DummyGameLogic;
public:
	explicit CustomAnimGraph(const Asset<AnimationClipAssetData> &clip0, const Asset<AnimationClipAssetData> &clip1);

	// AnimationGraph interface

	void preExecute(const AnimationGraphContext *context, float deltaTime) noexcept override;
	void execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept override;
	void postExecute(const AnimationGraphContext *context, float deltaTime) noexcept override;
	bool isActive() const noexcept override;

private:
	Asset<AnimationClipAssetData> m_clip0;
	Asset<AnimationClipAssetData> m_clip1;
	float m_duration = 1.0f;
	float m_time = 0.0f;
	float m_blend = 0.0f;
	bool m_active = true;
	bool m_paused = false;
	bool m_loop = true;;
};