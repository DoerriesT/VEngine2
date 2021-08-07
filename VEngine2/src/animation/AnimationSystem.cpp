#include "AnimationSystem.h"
#include "ecs/ECS.h"
#include "component/SkinnedMeshComponent.h"
#include "Skeleton.h"
#include "AnimationClip.h"
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <EASTL/fixed_vector.h>
#include "Log.h"

AnimationSystem::AnimationSystem(ECS *ecs) noexcept
	:m_ecs(ecs),
	m_skeletons(16),
	m_animationClips(16)
{
}

AnimationSystem::~AnimationSystem() noexcept
{
	for (auto *clip : m_animationClips)
	{
		delete clip;
	}
	m_animationClips.clear();

	for (auto *skele : m_skeletons)
	{
		delete skele;
	}
	m_skeletons.clear();
}

void AnimationSystem::update(float deltaTime) noexcept
{
	m_ecs->iterate<SkinnedMeshComponent>([this, deltaTime](size_t count, const EntityID *entities, SkinnedMeshComponent *skinnedMeshC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &smc = skinnedMeshC[i];
				
				Skeleton *skel = m_skeletons[smc.m_skeleton->getSkeletonHandle() - 1];
				AnimationClip *clip = m_animationClips[smc.m_animationClip->getAnimationClipHandle() - 1];

				// handle time and looping
				if (smc.m_playing && smc.m_time >= clip->getDuration())
				{
					if (smc.m_loop)
					{
						smc.m_time = fmodf(smc.m_time, clip->getDuration());
					}
					else
					{
						smc.m_playing = false;
						smc.m_time = 0.0f;
					}
				}

				// compute matrix palette for this frame
				if (smc.m_playing)
				{
					size_t jointCount = skel->getJointCount();
					const uint32_t *parentIndices = skel->getParentIndices();
					const glm::mat4 *invBindMatrices = skel->getInvBindPoseMatrices();

					// ensure the vector is correctly sized
					if (smc.m_matrixPalette.empty() || smc.m_matrixPalette.size() != jointCount)
					{
						smc.m_matrixPalette.resize(jointCount);
					}

					eastl::vector<glm::mat4> globalPoses(jointCount);

					// compute the matrix palette
					for (size_t i = 0; i < jointCount; ++i)
					{
						JointPose pose = clip->getJointPose(i, smc.m_time);

						glm::mat4 localPose =
							glm::translate(glm::make_vec3(pose.m_trans))
							* glm::mat4_cast(glm::make_quat(pose.m_rot))
							* glm::scale(glm::vec3(pose.m_scale));

						const uint32_t parentIdx = parentIndices[i];
						if (parentIdx != -1)
						{
							assert(parentIdx < i);
							localPose = globalPoses[parentIdx] * localPose;
						}

						globalPoses[i] = localPose;

						smc.m_matrixPalette[i] = globalPoses[i] * invBindMatrices[i];
					}

					// add delta time to our animation time
					smc.m_time += deltaTime;
				}
			}
		});
}

AnimationSkeletonHandle AnimationSystem::createSkeleton(size_t fileSize, const char *data) noexcept
{
	AnimationSkeletonHandle handle = (AnimationSkeletonHandle)m_skeletonHandleManager.allocate();

	if (!handle)
	{
		Log::warn("AnimationSystem: Failed to allocate AnimationSkeletonHandle!");
		return handle;
	}

	if (handle >= m_skeletons.size())
	{
		m_skeletons.resize((size_t)(m_skeletons.size() * 1.5));
	}

	m_skeletons[handle - 1] = new Skeleton(fileSize, data);

	return handle;
}

void AnimationSystem::destroySkeleton(AnimationSkeletonHandle handle) noexcept
{
	// LOCK_HOLDER(m_materialsMutex);
	{
		const bool validHandle = handle != 0 && handle <= m_skeletons.size();

		if (!validHandle)
		{
			return;
		}

		// call destructor and free backing memory
		delete m_skeletons[handle - 1];
		m_skeletons[handle - 1] = nullptr;

		{
			//LOCK_HOLDER(m_handleManagerMutex);
			m_skeletonHandleManager.free(handle);
		}
	}
}

AnimationClipHandle AnimationSystem::createAnimationClip(size_t fileSize, const char *data) noexcept
{
	AnimationClipHandle handle = (AnimationClipHandle)m_animationClipHandleManager.allocate();

	if (!handle)
	{
		Log::warn("AnimationSystem: Failed to allocate AnimationClipHandle!");
		return handle;
	}

	if (handle >= m_animationClips.size())
	{
		m_animationClips.resize((size_t)(m_animationClips.size() * 1.5));
	}

	m_animationClips[handle - 1] = new AnimationClip(fileSize, data);

	return handle;
}

void AnimationSystem::destroyAnimationClip(AnimationClipHandle handle) noexcept
{
	// LOCK_HOLDER(m_materialsMutex);
	{
		const bool validHandle = handle != 0 && handle <= m_animationClips.size();

		if (!validHandle)
		{
			return;
		}

		// call destructor and free backing memory
		delete m_animationClips[handle - 1];
		m_animationClips[handle - 1] = nullptr;

		{
			//LOCK_HOLDER(m_handleManagerMutex);
			m_animationClipHandleManager.free(handle);
		}
	}
}
