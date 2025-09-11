#pragma once
#include "Core.h"
#include "Job.h"
#include "JobDependency.h"

#include "ThreadContext.h"
#include "JobQueue.h"

#include "JobManagerQueue.h"

namespace djs
{
	class JobManagerConfig
	{
	public:
		int m_WorkerThreadCount = -1;
		// Physics
		uint32_t m_MaxPhysicsJobs = 4096;
		uint32_t m_MaxPhysicsBarriers = 16;

		std::vector<JobManagerQueue*> m_JobQueues{};
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
		)
		{
			return m_BaseJobsQueue.Schedule(
				*this,
				job,
				jobContextCount,
				jobElementCount,
				desiredBatchSize,
				dependecies,
				dependecyCount
			);
		}

		inline JobDependency Schedule(Job* job, JobDependency* dependecies = nullptr, uint64_t dependecyCount = 0)
		{
			return m_BaseJobsQueue.Schedule(
				*this,
				job,
				dependecies,
				dependecyCount
			);
		}

		inline JobDependency ScheduleParallelFor(
			JobParallelFor* job,
			int64_t elementCount,
			int64_t batchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		)
		{
			return m_BaseJobsQueue.ScheduleParallelFor(
				*this,
				job,
				elementCount,
				batchSize,
				dependecies,
				dependecyCount
			);
		}

		inline JobDependency ScheduleParallelForBatch(
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t maxBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		)
		{
			return m_BaseJobsQueue.ScheduleParallelForBatch(
				*this,
				job,
				elementCount,
				maxBatchSize,
				dependecies,
				dependecyCount
			);
		}

		inline JobDependency ScheduleParallelForBatch2(
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t maxBatches,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		)
		{
			return m_BaseJobsQueue.ScheduleParallelForBatch2(
				*this,
				job,
				elementCount,
				maxBatches,
				dependecies,
				dependecyCount
			);
		}

		inline JobDependency ScheduleParallelForBatch3(
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t minBatchSize,
			int64_t maxBatchCount,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		)
		{
			return m_BaseJobsQueue.ScheduleParallelForBatch3(
				*this,
				job,
				elementCount,
				minBatchSize,
				maxBatchCount,
				dependecies,
				dependecyCount
			);
		}

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

		std::unique_ptr<ThreadContext> m_MainThreadContext;

		std::vector<JobManagerQueue*> m_JobQueues{};
		std::vector<JobManagerQueue*> m_MainThreadJobQueues{};

	private:

		void Initialize(const JobManagerConfig& configuration);

		void Destroy();

	};
}
