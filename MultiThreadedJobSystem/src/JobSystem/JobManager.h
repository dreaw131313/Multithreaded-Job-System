#pragma once
#include "Core.h"
#include "Job.h"
#include "JobDependency.h"

#include "ThreadContext.h"
#include "JobQueue.h"

namespace JobSystem
{
	struct JobManagerConfig
	{
	public:
		int m_WorkerThreadCount = -1;

	};

	class JobManager
	{
	public:
		JobManager();

		JobManager(const JobManagerConfig& config);

		~JobManager();

		inline uint32_t GetWorkerThreadsCount() const
		{
			return m_WorkerThreadsCount;
		}

		void Initialize(const JobManagerConfig& config);

		void Destroy();

		void CompleteJobs();

		void WakeupThreads(int32_t threadsToWakeUp);

		inline uint32_t GetThreadIndex(uint64_t threadID) const
		{
			for (int32_t i = 0; i < m_WorkerThreadsCount; i++)
			{
				auto threadContext = m_WorkerThreadContexts[i];
				if (threadContext->GetThreadID() == threadID)
				{
					return threadContext->GetThreadIndex();
				}
			}

			return std::numeric_limits<uint32_t>::max();
		}

		JobDependency Schedule(
			JobBase* job,
			int64_t jobContextCount = 1,
			int64_t jobElementCount = 0,
			int64_t desiredBatchSize = 0,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency Schedule(Job* job, JobDependency* dependecies = nullptr, uint64_t dependecyCount = 0);

		JobDependency ScheduleParallelFor(
			JobParallelFor* job,
			int64_t elementCount,
			int64_t batchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelForBatch(
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t maxBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelForBatch2(
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t maxBatches,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		// Performs job until jobs queues are empty.
		void PerformJobsOnMainThread();

		// Perform jobs until jobs on dependecy is completed
		void PerformJobsOnMainThreadUntilDepenedcyCompleted(JobDependency& dependecy);

	private:
		JobQueue m_BaseJobsQueue = {};
		std::vector<ThreadContext*> m_WorkerThreadContexts;
		std::vector<std::thread> m_WorkerThreads;
		int32_t m_WorkerThreadsCount = 0;

		uint32_t m_LastWakedThreadIndex = 0;

		bool m_bIsInitialized = false;

		ThreadContext* m_MainThreadContext;

	};
}
