#include "CustomAnimGraph.h"
#include <animation/AnimationClip.h>
#include <utility/Utility.h>

CustomAnimGraph::CustomAnimGraph(
	const Asset<AnimationClipAssetData> &idle,
	const Asset<AnimationClipAssetData> &walk,
	const Asset<AnimationClipAssetData> &run
) noexcept
	:m_idleClip(idle),
	m_walkClip(walk),
	m_runClip(run)
{
}

void CustomAnimGraph::preExecute(const AnimationGraphContext *context, float deltaTime) noexcept
{
}

void CustomAnimGraph::execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept
{
	const auto *idleClip = context->getAnimationClip(m_idleClip->getAnimationClipHandle());
	const auto *walkClip = context->getAnimationClip(m_walkClip->getAnimationClipHandle());
	const auto *runClip = context->getAnimationClip(m_runClip->getAnimationClipHandle());

	JointPose inputPoses[3];
	inputPoses[0] = idleClip->getJointPose(jointIndex, m_phase * idleClip->getDuration(), m_loop);
	inputPoses[1] = walkClip->getJointPose(jointIndex, m_phase * walkClip->getDuration(), m_loop);
	inputPoses[2] = runClip->getJointPose(jointIndex, m_phase * runClip->getDuration(), m_loop);

	float speedKeys[] = { 0.0f, 0.3f, 1.0f };

	*resultJointPose = JointPose::lerp1DArray(3, inputPoses, speedKeys, m_speed);
}

void CustomAnimGraph::postExecute(const AnimationGraphContext *context, float deltaTime) noexcept
{
	if (!m_paused)
	{
		const AnimationClip *clips[]
		{
			context->getAnimationClip(m_idleClip->getAnimationClipHandle()),
			context->getAnimationClip(m_walkClip->getAnimationClipHandle()),
			context->getAnimationClip(m_runClip->getAnimationClipHandle())
		};

		float speedKeys[] = { 0.0f, 0.3f, 1.0f };

		size_t index0;
		size_t index1;
		float alpha;
		util::findPieceWiseLinearCurveIndicesAndAlpha(3, speedKeys, m_speed, false, &index0, &index1, &alpha);

		float duration = clips[index0]->getDuration() * (1.0f - alpha) + clips[index1]->getDuration() * alpha;

		m_phase += deltaTime / duration;
	}

	// handle time and looping
	if (m_phase > 1.0f)
	{
		if (m_loop)
		{
			m_phase = fmodf(m_phase, 1.0f);
		}
		else
		{
			m_active = false;
			m_phase = 0.0f;
		}
	}
}

bool CustomAnimGraph::isActive() const noexcept
{
	return m_active;
}
