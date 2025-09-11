#pragma once
#include "Core.h"
#include "Job.h"
#include "JobDependency.h"

#include "ThreadContext.h"
#include "JobQueue.h"

#include "JobSystemQueue.h"

namespace JobSystem
{
	class JobManagerConfig
	{
	public:
		int m_WorkerThreadCount = -1;
		// Physics
		uint32_t m_MaxPhysicsJobs = 4096;
		uint32_t m_MaxPhysicsBarriers = 16;

		std::vector<JobSystemQueue*> m_JobQueues{};
	};

	class JobManager
	{
	public:
		JobManager();

		JobManager(const JobManagerConfig& configuration);

		~JobManager();

		JobManager(const JobManager&) = delete;
		JobManager(JobManager&&) noexcept = delete;
		JobManager& operator=(const JobManager&) = delete;
		JobManager& operator=(JobManager&&)noexcept = delete;

		inline uint32_t GetWorkerThreadsCount() const
		{
			return m_WorkerThreadsCount;
		}

		inline std::thread::id GetThreadID(int32_t threadIndex) const
		{
			if (threadIndex < m_WorkerThreadsCount)
			{
				return m_WorkerThreads[threadIndex].get_id();
			}

			return  {};
		}

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

		JobDependency ScheduleParallelForBatch3(
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t minBatchSize,
			int64_t maxBatchCount,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		// Performs jobs until jobs queues are empty.
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

		std::unique_ptr<ThreadContext> m_MainThreadContext;

		std::vector<JobSystemQueue*> m_JobQueues{};
		std::vector<JobSystemQueue*> m_MainThreadJobQueues{};

	private:

		void Initialize(const JobManagerConfig& configuration);

		void Destroy();

	};
}
