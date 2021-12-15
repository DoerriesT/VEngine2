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
#include "job/ParallelFor.h"

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
			auto animateEntities = [&](size_t startIdx, size_t endIdx)
			{
				PROFILING_ZONE_SCOPED_N("Animate Entities");

				for (size_t entityIdx = startIdx; entityIdx != endIdx; ++entityIdx)
				{
					PROFILING_ZONE_SCOPED_N("Animate Entity");

					PROFILING_ZONE_BEGIN_N(profilingZoneAnimatePrepare, "Animate Prepare");

					auto &smc = skinnedMeshC[entityIdx];

					// compute matrix palette for this frame
					{
						const Skeleton *skel = smc.m_skeleton->getSkeleton();
						size_t jointCount = skel->getJointCount();
						const uint32_t *parentIndices = skel->getParentIndices();
						const glm::mat4 *invBindMatrices = skel->getInvBindPoseMatrices();

						// ensure the vector is correctly sized
						bool firstTimePalette = false;
						if (smc.m_matrixPalette.empty() || smc.m_matrixPalette.size() != jointCount)
						{
							smc.m_matrixPalette.resize(jointCount);
							smc.m_prevMatrixPalette.resize(jointCount);
							smc.m_lastRenderMatrixPalette.resize(jointCount);
							firstTimePalette = true;
						}

						if (!firstTimePalette)
						{
							smc.m_prevMatrixPalette = smc.m_matrixPalette;
							//eastl::swap(smc.m_matrixPalette, smc.m_prevMatrixPalette);
						}

						// early out if the graph is invalid
						if (!smc.m_animationGraph->isValid() || !smc.m_animationGraph->isLoaded())
						{
							for (size_t j = 0; j < jointCount; ++j)
							{
								smc.m_matrixPalette[j] = glm::identity<glm::mat4>();
							}
							PROFILING_ZONE_END(profilingZoneAnimatePrepare);
							continue;
						}

						smc.m_animationGraph->preEvaluate(m_ecs, entities[entityIdx], deltaTime);

						PROFILING_ZONE_END(profilingZoneAnimatePrepare);

						// compute local poses (can be done in parallel)
						auto computeLocalPoses = [&](size_t startIdx, size_t endIdx)
						{
							PROFILING_ZONE_SCOPED_N("Compute Local Poses");
							for (size_t j = startIdx; j < endIdx; ++j)
							{
								JointPose pose = smc.m_animationGraph->evaluate(j);

								glm::mat4 localPose =
									glm::translate(glm::make_vec3(pose.m_trans))
									* glm::mat4_cast(glm::make_quat(pose.m_rot))
									* glm::scale(glm::vec3(pose.m_scale));

								smc.m_matrixPalette[j] = localPose;
							}
						};

						job::parallelFor(jointCount, 1, computeLocalPoses);

						smc.m_animationGraph->updatePhase(deltaTime);

						PROFILING_ZONE_SCOPED_N("Compute Global Pose");

						// compute global poses (cant be done in parallel)
						// note that we skip the first element because it is the root element and has no parent
						for (size_t i = 1; i < jointCount; ++i)
						{
							const uint32_t parentIdx = parentIndices[i];
							assert(parentIdx < i);

							smc.m_matrixPalette[i] = smc.m_matrixPalette[parentIdx] * smc.m_matrixPalette[i];
						}

						// apply inverse bind matrices to global poses
						for (size_t i = 0; i < jointCount; ++i)
						{
							smc.m_matrixPalette[i] = smc.m_matrixPalette[i] * invBindMatrices[i];
						}

						// copy current palette into previous one if the palette was newly created this frame
						if (firstTimePalette)
						{
							memcpy(smc.m_prevMatrixPalette.data(), smc.m_matrixPalette.data(), sizeof(glm::mat4) * jointCount);
						}
					}
				}
			};

			job::parallelFor(count, 1, animateEntities);
		});
}