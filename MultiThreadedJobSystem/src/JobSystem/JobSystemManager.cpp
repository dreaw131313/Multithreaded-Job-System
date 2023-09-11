
#include "JobSystemManager.h"
#include <iostream>

namespace JobSystem
{
	JobSystemManager::JobSystemManager()
	{
	}

	JobSystemManager::~JobSystemManager()
	{
	}

	void JobSystemManager::Initialize(uint32_t maxWorkerThreads)
	{
		if (m_bIsInitialized)
		{
			return;
		}
		m_bIsInitialized = true;

		int workerThreadCount = std::thread::hardware_concurrency() - 1;
		if (workerThreadCount <= 0)
		{
			//LOG_WARN("Failed to initialize threads in JobSystemManagerInstance, function \"std::thread::hardware_concurrency()\" returns value \"0\"");
			return;
		}

		if (maxWorkerThreads > 0)
		{
			m_ThreadsCount = std::min((uint32_t)workerThreadCount, maxWorkerThreads);
		}
		else
		{
			m_ThreadsCount = workerThreadCount;
		}

		auto threadLoop = [](JobsThreadContext* context, JobQueue* jobQueue)
		{
			if (jobQueue == nullptr || context == nullptr)
			{
				return;
			}

			JobQueue::JobRecordData acquireJobResult = {};

			do
			{
				// get job from queue and remove from it
				acquireJobResult = jobQueue->GetRemoveJob();

				// if no job in queue then sleep:
				if (acquireJobResult.m_Job == nullptr)
				{
					context->Sleep();
				}

				// after wake up skip couse job is still nullptr
				// after getting job execute and complete it and reset acquire job result
				if (acquireJobResult.m_Job != nullptr)
				{
					acquireJobResult.m_Job->Execute(
						acquireJobResult.m_JobContextIndex,
						acquireJobResult.m_JobElementCount,
						acquireJobResult.m_DesiredBatchSize,
						*context
					);
					acquireJobResult.m_JobDependecy->SetCompleted();
					acquireJobResult.Reset();
				}
			}
			while (context->IsAlive());
		};

		for (uint32_t i = 0; i < m_ThreadsCount; i++)
		{
			auto context = new JobsThreadContext(i);
			m_ThreadContexts.push_back(context);
			m_Threads.emplace_back(threadLoop, context, &m_BaseJobsQueue);
		}
	}

	void JobSystemManager::Destroy()
	{
		if (!m_bIsInitialized)
		{
			return;
		}
		m_bIsInitialized = false;

		CompleteJobs();

		// setting is alive flags to null
		for (uint32_t i = 0; i < m_ThreadContexts.size(); i++)
		{
			m_ThreadContexts[i]->Kill();
			m_ThreadContexts[i]->WakeUp();
		}

		// joining threads
		for (uint32_t i = 0; i < m_Threads.size(); i++)
		{
			m_Threads[i].join();
		}

		for (uint32_t i = 0; i < m_ThreadContexts.size(); i++)
		{
			delete m_ThreadContexts[i];
		}

		// clearing containers:
		m_ThreadContexts.clear();
		m_Threads.clear();

		// clearing queue:
		m_BaseJobsQueue.Clear();
	}

	void JobSystemManager::CompleteJobs()
	{
		while (m_BaseJobsQueue.GetJobsCount() > 0);

		uint32_t sleepingthreads = 0;
		while (sleepingthreads != m_ThreadsCount)
		{
			sleepingthreads = 0;
			for (auto threadContext : m_ThreadContexts)
			{
				if (!threadContext->IsAwake())
				{
					sleepingthreads += 1;
				}
			}
		}
	}

	JobDependency JobSystemManager::Schedule(
		JobBase* job,
		int64_t jobContextCount,
		int64_t jobElementCount,
		int64_t desiredBatchSize,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (job != nullptr && jobContextCount > 0)
		{
			std::shared_ptr<JobDependencyData> jobDependecyData = std::make_shared<JobDependencyData>((int)jobContextCount);
			m_BaseJobsQueue.AddJob(
				job,
				jobDependecyData,
				jobElementCount,
				desiredBatchSize,
				dependecies,
				dependecyCount
			);

			// wakeup correct number of threads:
			WakeupThreads((uint32_t)jobContextCount);

			return JobDependency(jobDependecyData);
		}

		return JobDependency();
	}

	JobDependency JobSystemManager::Schedule(Job* job, JobDependency* dependecies, uint64_t dependecyCount)
	{
		return Schedule(job, 1, -1, -1, dependecies, dependecyCount);
	}

	JobDependency JobSystemManager::ScheduleParallelFor(
		JobParallelFor* job,
		int64_t elementCount,
		int64_t batchSize,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (elementCount > 0 && batchSize > 0)
		{
			int64_t contextCount = elementCount / batchSize;
			int64_t modulo = elementCount % batchSize;
			if (modulo > 0)
			{
				contextCount += 1;
			}

			return Schedule(job, contextCount, elementCount, batchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	JobDependency JobSystemManager::ScheduleParallelForBatch(
		JobParallelForBatch* job,
		int64_t elementCount,
		int64_t maxBatchSize,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (elementCount > 0 && maxBatchSize > 0)
		{
			if (maxBatchSize >= elementCount)
			{
				return Schedule(job, 1, elementCount, elementCount, dependecies, dependecyCount);
			}

			int64_t contextCount = elementCount / maxBatchSize;
			int64_t modulo = elementCount % maxBatchSize;
			if (modulo > 0)
			{
				contextCount += 1;
			}

			return Schedule(job, contextCount, elementCount, maxBatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	JobDependency JobSystemManager::ScheduleParallelForBatch2(
		JobParallelForBatch* job,
		int64_t elementCount,
		int64_t maxBatches,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (elementCount > 0 && maxBatches > 0)
		{
			if (maxBatches == 1)
			{
				return Schedule(job, 1, elementCount, elementCount, dependecies, dependecyCount);
			}
			else if (elementCount == maxBatches)
			{
				return Schedule(job, maxBatches, elementCount, 1, dependecies, dependecyCount);
			}

			int64_t contextCount = maxBatches;
			int64_t desiredBatchSize = elementCount / contextCount;
			int64_t checkedElementsCount = desiredBatchSize * contextCount;

			if (elementCount > checkedElementsCount)
			{
				desiredBatchSize += 1;
				contextCount = elementCount / desiredBatchSize;
			}

			int64_t modulo = elementCount % (desiredBatchSize * contextCount);

			if (modulo > 0)
			{
				contextCount += 1;
			}

			return Schedule(job, contextCount, elementCount, desiredBatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	void JobSystemManager::WakeupThreads(uint32_t threadsToWakeUp)
	{
		uint64_t maxThreads = std::max(threadsToWakeUp, m_ThreadsCount);
		for (uint64_t i = 0; i < threadsToWakeUp; i++)
		{
			m_ThreadContexts[m_LastWakedThreadIndex]->WakeUp();
			m_LastWakedThreadIndex = (m_LastWakedThreadIndex + 1) % m_ThreadsCount;
		}
	}

}