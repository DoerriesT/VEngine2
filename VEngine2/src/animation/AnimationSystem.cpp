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
#include "AnimationGraph.h"
#include "profiling/Profiling.h"

AnimationSystem::AnimationSystem(ECS *ecs) noexcept
	:m_ecs(ecs)
{
}

AnimationSystem::~AnimationSystem() noexcept
{
}

void AnimationSystem::update(float deltaTime) noexcept
{
	PROFILING_ZONE_SCOPED;

	m_ecs->iterate<SkinnedMeshComponent>([this, deltaTime](size_t count, const EntityID *entities, SkinnedMeshComponent *skinnedMeshC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &smc = skinnedMeshC[i];

				// compute matrix palette for this frame
				{
					const Skeleton *skel = smc.m_skeleton->getSkeleton();
					size_t jointCount = skel->getJointCount();
					const uint32_t *parentIndices = skel->getParentIndices();
					const glm::mat4 *invBindMatrices = skel->getInvBindPoseMatrices();

					// ensure the vector is correctly sized
					if (smc.m_matrixPalette.empty() || smc.m_matrixPalette.size() != jointCount)
					{
						smc.m_matrixPalette.resize(jointCount);
					}

					// early out if the graph is invalid
					if (!smc.m_animationGraph->isValid() || !smc.m_animationGraph->isLoaded())
					{
						for (size_t j = 0; j < jointCount; ++j)
						{
							smc.m_matrixPalette[j] = glm::identity<glm::mat4>();
						}
						continue;
					}

					smc.m_animationGraph->preEvaluate(m_ecs, entities[i], deltaTime);

					// compute local poses (can be done in parallel)
					eastl::vector<glm::mat4> localPoses(jointCount);
					for (size_t i = 0; i < jointCount; ++i)
					{
						JointPose pose = smc.m_animationGraph->evaluate(i);

						glm::mat4 localPose =
							glm::translate(glm::make_vec3(pose.m_trans))
							* glm::mat4_cast(glm::make_quat(pose.m_rot))
							* glm::scale(glm::vec3(pose.m_scale));

						localPoses[i] = localPose;
					}

					smc.m_animationGraph->updatePhase(deltaTime);

					// compute global poses and matrix palette (cant be done in parallel)
					eastl::vector<glm::mat4> globalPoses(jointCount);
					for (size_t i = 0; i < jointCount; ++i)
					{
						const uint32_t parentIdx = parentIndices[i];
						if (parentIdx != -1)
						{
							assert(parentIdx < i);
							globalPoses[i] = globalPoses[parentIdx] * localPoses[i];
						}
						else
						{
							globalPoses[i] = localPoses[i];
						}

						smc.m_matrixPalette[i] = globalPoses[i] * invBindMatrices[i];
					}
				}
			}
		});
}