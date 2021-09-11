#include "CustomAnimGraph.h"
#include <animation/AnimationClip.h>
#include <utility/Utility.h>
#include <glm/gtc/type_ptr.hpp>
#include <asset/AssetManager.h>
#include <asset/AnimationClipAsset.h>
#include <component/CharacterMovementComponent.h>
#include <ecs/ECS.h>

static const float s_speedKeys[] = { 0.0f, 2.0f, 6.0f };

CustomAnimGraph::CustomAnimGraph() noexcept
	:m_idleClip(AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/idle.anim"))),
	m_walkClip(AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/walk0.anim"))),
	m_runClip(AssetManager::get()->getAsset<AnimationClipAssetData>(SID("meshes/run.anim")))
{
}

void CustomAnimGraph::preExecute(ECS *ecs, EntityID entity, float deltaTime) noexcept
{
	const auto &mc = ecs->getComponent<CharacterMovementComponent>(entity);
	m_speed = glm::length(glm::vec2(mc->m_velocityX, mc->m_velocityZ));
}

void CustomAnimGraph::execute(ECS *ecs, EntityID entity, size_t jointIndex, float deltaTime, JointPose *resultJointPose) const noexcept
{
	const auto *idleClip = m_idleClip->getAnimationClip();
	const auto *walkClip = m_walkClip->getAnimationClip();
	const auto *runClip = m_runClip->getAnimationClip();

	const bool extractRootMotion = false;
	const bool loop = true;

	JointPose walkPose = walkClip->getJointPose(jointIndex, m_phase * walkClip->getDuration(), loop, extractRootMotion);
	JointPose runPose = runClip->getJointPose(jointIndex, m_phase * runClip->getDuration(), loop, extractRootMotion);

	JointPose inputPoses[3];
	inputPoses[0] = idleClip->getJointPose(jointIndex, m_phase * idleClip->getDuration(), loop, extractRootMotion);
	inputPoses[1] = walkPose;
	inputPoses[2] = runPose;

	*resultJointPose = JointPose::lerp1DArray(3, inputPoses, s_speedKeys, fabsf(m_speed));
}

void CustomAnimGraph::postExecute(ECS *ecs, EntityID entity, float deltaTime) noexcept
{
	if (!m_paused)
	{
		const AnimationClip *clips[]
		{
			m_idleClip->getAnimationClip(),
			m_walkClip->getAnimationClip(),
			m_runClip->getAnimationClip()
		};

		size_t index0;
		size_t index1;
		float alpha;
		util::findPieceWiseLinearCurveIndicesAndAlpha(3, s_speedKeys, fabsf(m_speed), false, &index0, &index1, &alpha);

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
		m_phase = glm::fract(m_phase);
	}
}

bool CustomAnimGraph::isActive() const noexcept
{
	return m_active;
}
