#pragma once
#include <animation/AnimationGraph.h>
#include <asset/AnimationClipAsset.h>

class CustomAnimGraph : public AnimationGraph
{
	friend class DummyGameLogic;
public:
	explicit CustomAnimGraph(
		const Asset<AnimationClipAssetData> &idle,
		const Asset<AnimationClipAssetData> &walk,
		const Asset<AnimationClipAssetData> &run,
		const Asset<AnimationClipAssetData> &strafeLeftWalkClip,
		const Asset<AnimationClipAssetData> &strafeRightWalkClip,
		const Asset<AnimationClipAssetData> &strafeLeftRunClip,
		const Asset<AnimationClipAssetData> &strafeRightRunClip
	) noexcept;

	float getRootMotionSpeed() const noexcept;

	// AnimationGraph interface

	void preExecute(const AnimationGraphContext *context, float deltaTime) noexcept override;
	void execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept override;
	void postExecute(const AnimationGraphContext *context, float deltaTime) noexcept override;
	bool isActive() const noexcept override;

private:
	Asset<AnimationClipAssetData> m_idleClip;
	Asset<AnimationClipAssetData> m_walkClip;
	Asset<AnimationClipAssetData> m_runClip;
	Asset<AnimationClipAssetData> m_strafeLeftWalkClip;
	Asset<AnimationClipAssetData> m_strafeRightWalkClip;
	Asset<AnimationClipAssetData> m_strafeLeftRunClip;
	Asset<AnimationClipAssetData> m_strafeRightRunClip;
	float m_phase = 0.0f;
	float m_speed = 0.0f;
	float m_strafe = 0.0f;
	bool m_active = true;
	bool m_paused = false;
	bool m_loop = true;
	float m_rootMotionSpeed = 0.0f;
};