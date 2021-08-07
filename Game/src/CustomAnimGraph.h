#pragma once
#include <animation/AnimationGraph.h>
#include <asset/AnimationClipAsset.h>


class CustomAnimGraph : public AnimationGraph
{
	friend class DummyGameLogic;
public:
	void preExecute(const AnimationGraphContext *context, float deltaTime) noexcept override;
	void execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept override;
	void postExecute(const AnimationGraphContext *context, float deltaTime) noexcept override;
	bool isActive() const noexcept override;

private:
	Asset<AnimationClipAssetData> m_clip0;
	Asset<AnimationClipAssetData> m_clip1;
	float m_duration;
	float m_time;
	float m_blend;
	bool m_active;
	bool m_paused;
	bool m_loop;
};