#pragma once
#include "JobSystem.h"
#include <EASTL/algorithm.h>
#include "utility/Memory.h"
#include "profiling/Profiling.h"

namespace job
{
	template<typename F>
	void parallelFor(size_t count, size_t minBatchSize, const F &func)
	{
		struct ParallelForJobData
		{
			size_t m_startIdx;
			size_t m_endIdx;
			const F *m_func;
		};

		// early exit
		if (count == 0)
		{
			return;
		}
		else if (count == 1)
		{
			func(0, 1);
			return;
		}

		const size_t numWorkers = job::getThreadCount();
		const size_t batchSize = eastl::max<size_t>(minBatchSize, (count + numWorkers - 1) / numWorkers);
		const size_t jobCount = (count + (batchSize - 1)) / batchSize;

		job::Job *jobs = ALLOC_A_T(job::Job, jobCount);
		ParallelForJobData *jobData = ALLOC_A_T(ParallelForJobData, jobCount);

		for (size_t i = 0; i < jobCount; ++i)
		{
			jobData[i].m_startIdx = i * batchSize;
			jobData[i].m_endIdx = eastl::min<size_t>(jobData[i].m_startIdx + batchSize, count);
			jobData[i].m_func = &func;

			auto jobFunc = [](void *arg)
			{
				ParallelForJobData *data = (ParallelForJobData *)arg;
				(*(data->m_func))(data->m_startIdx, data->m_endIdx);
			};

			jobs[i] = job::Job(jobFunc, &(jobData[i]));
		}

		{
			PROFILING_ZONE_SCOPED_N("Parallel For Kick and Wait");
			job::Counter *counter = nullptr;
			job::run(jobCount, jobs, &counter);
			job::waitForCounter(counter);
			job::freeCounter(counter);
		}
	}
}