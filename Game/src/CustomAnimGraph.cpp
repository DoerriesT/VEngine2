#include "CustomAnimGraph.h"
#include <animation/AnimationClip.h>
#include <utility/Utility.h>
#include <glm/gtc/type_ptr.hpp>

CustomAnimGraph::CustomAnimGraph(
	const Asset<AnimationClipAssetData> &idle,
	const Asset<AnimationClipAssetData> &walk,
	const Asset<AnimationClipAssetData> &run,
	const Asset<AnimationClipAssetData> &strafeLeftWalkClip,
	const Asset<AnimationClipAssetData> &strafeRightWalkClip,
	const Asset<AnimationClipAssetData> &strafeLeftRunClip,
	const Asset<AnimationClipAssetData> &strafeRightRunClip
) noexcept
	:m_idleClip(idle),
	m_walkClip(walk),
	m_runClip(run),
	m_strafeLeftWalkClip(strafeLeftWalkClip),
	m_strafeRightWalkClip(strafeRightWalkClip),
	m_strafeLeftRunClip(strafeLeftRunClip),
	m_strafeRightRunClip(strafeRightRunClip)
{
}

float CustomAnimGraph::getRootMotionSpeed() const noexcept
{
	return m_rootMotionSpeed;
}

void CustomAnimGraph::preExecute(const AnimationGraphContext *context, float deltaTime) noexcept
{
}

void CustomAnimGraph::execute(const AnimationGraphContext *context, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept
{
	const auto *idleClip = context->getAnimationClip(m_idleClip->getAnimationClipHandle());
	const auto *walkClip = context->getAnimationClip(m_walkClip->getAnimationClipHandle());
	const auto *runClip = context->getAnimationClip(m_runClip->getAnimationClipHandle());
	const auto *strafeLeftWalkClip = context->getAnimationClip(m_strafeLeftWalkClip->getAnimationClipHandle());
	const auto *strafeRightWalkClip = context->getAnimationClip(m_strafeRightWalkClip->getAnimationClipHandle());
	const auto *strafeLeftRunClip = context->getAnimationClip(m_strafeLeftRunClip->getAnimationClipHandle());
	const auto *strafeRightRunClip = context->getAnimationClip(m_strafeRightRunClip->getAnimationClipHandle());

	const bool extractRootMotion = false;

	JointPose walkPose;
	{
		JointPose inputPoses[3];
		inputPoses[0] = strafeLeftWalkClip->getJointPose(jointIndex, m_phase * strafeLeftWalkClip->getDuration(), m_loop, extractRootMotion);
		inputPoses[1] = walkClip->getJointPose(jointIndex, m_phase * walkClip->getDuration(), m_loop, extractRootMotion);
		inputPoses[2] = strafeRightWalkClip->getJointPose(jointIndex, m_phase * strafeRightWalkClip->getDuration(), m_loop, extractRootMotion);

		float strafeKeys[] = { -1.0f, 0.0f, 1.0f };

		walkPose = JointPose::lerp1DArray(3, inputPoses, strafeKeys, m_speed >= 0.0f ? m_strafe : -m_strafe);
	}

	JointPose runPose;
	{
		JointPose inputPoses[3];
		inputPoses[0] = strafeLeftRunClip->getJointPose(jointIndex, m_phase * strafeLeftRunClip->getDuration(), m_loop, extractRootMotion);
		inputPoses[1] = runClip->getJointPose(jointIndex, m_phase * runClip->getDuration(), m_loop, extractRootMotion);
		inputPoses[2] = strafeRightRunClip->getJointPose(jointIndex, m_phase * strafeRightRunClip->getDuration(), m_loop, extractRootMotion);

		float strafeKeys[] = { -1.0f, 0.0f, 1.0f };

		runPose = JointPose::lerp1DArray(3, inputPoses, strafeKeys, m_speed >= 0.0f ? m_strafe : -m_strafe);
	}

	JointPose inputPoses[3];
	inputPoses[0] = idleClip->getJointPose(jointIndex, m_phase * idleClip->getDuration(), m_loop, extractRootMotion);
	inputPoses[1] = walkPose;
	inputPoses[2] = runPose;

	float speedKeys[] = { 0.0f, 0.3f, 1.0f };

	*resultJointPose = JointPose::lerp1DArray(3, inputPoses, speedKeys, fabsf(m_speed));
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
		util::findPieceWiseLinearCurveIndicesAndAlpha(3, speedKeys, fabsf(m_speed), false, &index0, &index1, &alpha);

		float rootVelocity0[3];
		float rootVelocity1[3];
		clips[index0]->getRootVelocity(m_phase * clips[index0]->getDuration(), m_loop, rootVelocity0);
		clips[index1]->getRootVelocity(m_phase * clips[index1]->getDuration(), m_loop, rootVelocity1);
		
		glm::vec3 rootVelocity = glm::mix(glm::make_vec3(rootVelocity0), glm::make_vec3(rootVelocity1), alpha) * deltaTime;

		rootVelocity.z = 0.0f;
		m_rootMotionSpeed = glm::length(rootVelocity);

		float duration = clips[index0]->getDuration() * (1.0f - alpha) + clips[index1]->getDuration() * alpha;

		if (m_speed >= 0.0f)
		{
			m_phase += deltaTime / duration;
		}
		else
		{
			m_phase -= deltaTime / duration;
		}
	}

	// handle time and looping
	if (m_phase > 1.0f || m_phase < 0.0f)
	{
		if (m_loop)
		{
			m_phase = glm::fract(m_phase);
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
