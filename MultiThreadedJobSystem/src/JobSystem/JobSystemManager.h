#pragma once
#include "Job.h"
#include "JobDependency.h"

#include "JobsThreadContext.h"
#include "JobQueue.h"

namespace JobSystem
{
	class JobSystemManager
	{
	public:
		JobSystemManager();

		~JobSystemManager();

		inline uint32_t GetThreadsCount() const
		{
			return m_ThreadsCount;
		}

		void Initialize(uint32_t maxWorkerThreads = 0);

		void Destroy();

		void CompleteJobs();

		JobDependency Schedule(
			JobBase* job,
			int64_t jobContextCount = 1,
			int64_t jobElementCount = 0,
			int64_t desiredBatchSize = 0,
			JobDependency* dependecies = nullptr, 
			uint64_t dependecyCount = 0
		);

		JobDependency Schedule(
			Job* job,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelFor(
			JobParallelFor* job,
			int64_t elementCount,
			int64_t maxBatchSize,
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
			int64_t batchCount,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

	private:
		JobQueue m_BaseJobsQueue = {};
		std::vector<JobsThreadContext*> m_ThreadContexts;
		std::vector<std::thread> m_Threads;
		uint32_t m_ThreadsCount = 0;

		uint32_t m_LastWakedThreadIndex = 0;

		bool m_bIsInitialized = false;
	private:
		void WakeupThreads(uint32_t threadsToWakeUp);
	};
}