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
#include "task/Task.h"

namespace
{
	struct AnimTaskData
	{
		size_t startIdx;
		size_t endIdx;
		float deltaTime;
		ECS *ecs;
		const EntityID *entities;
		SkinnedMeshComponent *comps;
	};

	struct LocalPoseTaskData
	{
		AnimationGraph *animationGraph;
		size_t startIdx;
		size_t endIdx;
		glm::mat4 *localPoses;
	};
}

static void computeLocalPosesTask(void *arg)
{
	PROFILING_ZONE_SCOPED_N("Compute Local Poses");

	LocalPoseTaskData *data = (LocalPoseTaskData *)arg;

	for (size_t j = data->startIdx; j != data->endIdx; ++j)
	{
		JointPose pose = data->animationGraph->evaluate(j);

		glm::mat4 localPose =
			glm::translate(glm::make_vec3(pose.m_trans))
			* glm::mat4_cast(glm::make_quat(pose.m_rot))
			* glm::scale(glm::vec3(pose.m_scale));

		data->localPoses[j] = localPose;
	}
}

static void animateEntitiesTask(void *arg)
{
	PROFILING_ZONE_SCOPED_N("Animate Entities");

	AnimTaskData *data = (AnimTaskData *)arg;

	for (size_t entityIdx = data->startIdx; entityIdx != data->endIdx; ++entityIdx)
	{
		PROFILING_ZONE_SCOPED_N("Animate Entity");

		PROFILING_ZONE_BEGIN_N(profilingZoneAnimatePrepare, "Animate Prepare");

		auto &smc = data->comps[entityIdx];

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
				PROFILING_ZONE_END(profilingZoneAnimatePrepare);
				continue;
			}

			smc.m_animationGraph->preEvaluate(data->ecs, data->entities[entityIdx], data->deltaTime);

			PROFILING_ZONE_END(profilingZoneAnimatePrepare);

			// compute local poses (can be done in parallel)
			const size_t numWorkers = task::getThreadCount();
			const size_t taskCount = eastl::min<size_t>(numWorkers, jointCount);// (jointCount + (stepSize - 1)) / stepSize;
			const size_t stepSize = (jointCount + (numWorkers - 1)) / numWorkers;
			eastl::fixed_vector<task::Task, 16> localPoseTasks(taskCount);
			eastl::fixed_vector<LocalPoseTaskData, 16> localPoseTaskData(taskCount);
			for (size_t i = 0; i < taskCount; ++i)
			{
				localPoseTaskData[i].animationGraph = smc.m_animationGraph;
				localPoseTaskData[i].startIdx = i * stepSize;
				localPoseTaskData[i].endIdx = eastl::min<size_t>(localPoseTaskData[i].startIdx + stepSize, jointCount);
				localPoseTaskData[i].localPoses = smc.m_matrixPalette.data();
			}

			for (size_t i = 0; i < taskCount; ++i)
			{
				//computeLocalPosesTask(&(localPoseTaskData[i]));
				localPoseTasks[i] = task::Task(computeLocalPosesTask, &(localPoseTaskData[i]), "computeLocalPosesTask");
			}

			{
				PROFILING_ZONE_SCOPED_N("Kick and Wait");
				task::Counter *counter = nullptr;
				task::run(taskCount, localPoseTasks.data(), &counter);
				task::waitForCounter(counter);
				task::freeCounter(counter);
			}


			smc.m_animationGraph->updatePhase(data->deltaTime);

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
		}
	}
}

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
			const size_t numWorkers = task::getThreadCount();
			const size_t taskCount = eastl::min<size_t>(numWorkers, count);// (jointCount + (stepSize - 1)) / stepSize;
			const size_t stepSize = (count + (numWorkers - 1)) / numWorkers;

			eastl::fixed_vector<task::Task, 16> animTasks(taskCount);
			eastl::fixed_vector<AnimTaskData, 16> animTaskData(taskCount);

			for (size_t i = 0; i < taskCount; ++i)
			{
				animTaskData[i].startIdx = i * stepSize;
				animTaskData[i].endIdx = eastl::min<size_t>(animTaskData[i].startIdx + stepSize, count);
				animTaskData[i].deltaTime = deltaTime;
				animTaskData[i].ecs = m_ecs;
				animTaskData[i].entities = entities;
				animTaskData[i].comps = skinnedMeshC;
			}

			for (size_t i = 0; i < taskCount; ++i)
			{
				//animateEntitiesTask(&(animTaskData[i]));
				animTasks[i] = task::Task(animateEntitiesTask, &(animTaskData[i]), "animateEntitiesTask");
			}

			{
				PROFILING_ZONE_SCOPED_N("Kick and Wait");
				task::Counter *counter = nullptr;
				task::run(taskCount, animTasks.data(), &counter);
				task::waitForCounter(counter);
				task::freeCounter(counter);
			}
		});
}