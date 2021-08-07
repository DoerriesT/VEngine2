#include "CustomAnimGraph.h"
#include <animation/AnimationClip.h>
#include <graphics/imgui/imgui.h>

CustomAnimGraph::CustomAnimGraph(const Asset<AnimationClipAssetData> &clip0, const Asset<AnimationClipAssetData> &clip1)
	:m_clip0(clip0),
	m_clip1(clip1)
{
}

void CustomAnimGraph::preExecute(const AnimationGraphContext *context, float deltaTime) noexcept
{
}

void CustomAnimGraph::execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept
{
	const auto *clip0 = context->getAnimationClip(m_clip0->getAnimationClipHandle());
	const auto *clip1 = context->getAnimationClip(m_clip1->getAnimationClipHandle());

	JointPose pose0 = clip0->getJointPose(jointIndex, m_time * clip0->getDuration(), m_loop);
	JointPose pose1 = clip1->getJointPose(jointIndex, m_time * clip1->getDuration(), m_loop);

	*resultJointPose = JointPose::lerp(pose0, pose1, m_blend);
}

void CustomAnimGraph::postExecute(const AnimationGraphContext *context, float deltaTime) noexcept
{
	if (!m_paused)
	{
		m_time += deltaTime / m_duration;
	}

	// handle time and looping
	if (m_time > 1.0f)
	{
		if (m_loop)
		{
			m_time = fmodf(m_time, 1.0f);
		}
		else
		{
			m_active = false;
			m_time = 0.0f;
		}
	}
}

bool CustomAnimGraph::isActive() const noexcept
{
	return m_active;
}
