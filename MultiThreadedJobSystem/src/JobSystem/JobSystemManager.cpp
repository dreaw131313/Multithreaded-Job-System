#include "JobSystemManager.h"

namespace JobSystem
{
	JobSystemManager::JobSystemManager()
	{
		Initialize();
	}

	JobSystemManager::JobSystemManager(int32_t maxWorkerThreads)
	{
		Initialize(maxWorkerThreads);
	}

	JobSystemManager::~JobSystemManager()
	{
		Destroy();
	}

	void JobSystemManager::Initialize(int32_t maxWorkerThreads)
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
			m_WorkerThreadsCount = std::min(workerThreadCount, maxWorkerThreads);
		}
		else
		{
			m_WorkerThreadsCount = workerThreadCount;
		}

		auto threadLoop = [this](ThreadContext* context, JobQueue* jobQueue)
		{
			if (jobQueue == nullptr || context == nullptr)
			{
				return;
			}

			context->m_ThreadID = std::hash<std::thread::id>()(std::this_thread::get_id());

			JobThreadData acquireJobResult = {};
			uint64_t jobCount = 0;

			while (context->IsAlive())
			{
				// get job from queue and remove from it
				uint64_t jobCount = 0;
				acquireJobResult = jobQueue->GetRemoveJob(jobCount);

				// after wake up skip couse job is still nullptr
				// after getting job execute and complete it and reset acquire job result
				if (acquireJobResult.m_Job != nullptr)
				{
					acquireJobResult.m_Job->Execute_Internal(
						acquireJobResult.m_JobContextIndex,
						acquireJobResult.m_JobElementCount,
						acquireJobResult.m_DesiredBatchSize,
						*context
					);
					acquireJobResult.m_JobDependecy->SetCompleted();
					acquireJobResult.Reset();
				}

				// if no job in queue then sleep:
				if (jobCount == 0)
				{
					context->Sleep();
				}

			}
		};

		for (int32_t i = 0; i < m_WorkerThreadsCount; i++)
		{
			auto context = new ThreadContext(i);
			m_WorkerThreadContexts.push_back(context);

			auto& thread = m_WorkerThreads.emplace_back(threadLoop, context, &m_BaseJobsQueue);
		}

		m_MainThreadContext = new ThreadContext(m_WorkerThreadsCount);
	}

	void JobSystemManager::Destroy()
	{
		if (!m_bIsInitialized)
		{
			return;
		}
		m_bIsInitialized = false;

		// setting is alive flags to null
		for (uint32_t i = 0; i < m_WorkerThreadContexts.size(); i++)
		{
			m_WorkerThreadContexts[i]->Kill();
			m_WorkerThreadContexts[i]->WakeUp();
		}

		// joining threads
		for (uint32_t i = 0; i < m_WorkerThreads.size(); i++)
		{
			m_WorkerThreads[i].join();
		}

		for (uint32_t i = 0; i < m_WorkerThreadContexts.size(); i++)
		{
			delete m_WorkerThreadContexts[i];
		}

		delete m_MainThreadContext;
		m_MainThreadContext = nullptr;

		// clearing containers:
		m_WorkerThreadContexts.clear();
		m_WorkerThreads.clear();

		// clearing queue:
		m_BaseJobsQueue.Clear();
	}

	void JobSystemManager::Reinitialize(int32_t maxWorkerThreads)
	{
		Destroy();
		Initialize(maxWorkerThreads);
	}

	void JobSystemManager::CompleteJobs()
	{
		while (m_BaseJobsQueue.GetJobsCount() > 0);

		int32_t sleepingthreads = 0;
		while (sleepingthreads != m_WorkerThreadsCount)
		{
			sleepingthreads = 0;
			for (auto threadContext : m_WorkerThreadContexts)
			{
				if (!threadContext->IsAwake())
				{
					sleepingthreads += 1;
				}
			}
		}
	}

	void JobSystemManager::WakeupThreads(int32_t threadsToWakeUp)
	{
		int32_t maxThreads = std::max(threadsToWakeUp, m_WorkerThreadsCount);
		for (int32_t i = 0; i < maxThreads; i++)
		{
			m_WorkerThreadContexts[m_LastWakedThreadIndex]->WakeUp();
			m_LastWakedThreadIndex = (m_LastWakedThreadIndex + 1) % m_WorkerThreadsCount;
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
				jobContextCount,
				jobElementCount,
				desiredBatchSize,
				dependecies,
				dependecyCount
			);

			// wakeup correct number of threads:
			WakeupThreads((int32_t)jobContextCount);

			return JobDependency(jobDependecyData, this);
		}

		return JobDependency();
	}

	JobDependency JobSystemManager::Schedule(Job* job, JobDependency* dependecies, uint64_t dependecyCount)
	{
		return Schedule(job, 1, -1, -1, dependecies, dependecyCount);
	}

	JobDependency JobSystemManager::ScheduleParallelFor(JobParallelFor* job, int64_t elementCount, int64_t batchSize, JobDependency* dependecies, uint64_t dependecyCount)
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

	JobDependency JobSystemManager::ScheduleParallelForBatch(JobParallelForBatch* job, int64_t elementCount, int64_t maxBatchSize, JobDependency* dependecies, uint64_t dependecyCount)
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

	JobDependency JobSystemManager::ScheduleParallelForBatch2(JobParallelForBatch* job, int64_t elementCount, int64_t maxBatches, JobDependency* dependecies, uint64_t dependecyCount)
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

	void JobSystemManager::PerformJobsOnMainThread()
	{
		JobThreadData acquireJobResult = {};
		uint64_t jobCount = 0;

		while (m_BaseJobsQueue.GetJobsCount() == 0)
		{
			// get job from queue and remove from it
			acquireJobResult = m_BaseJobsQueue.GetRemoveJob(jobCount);

			// after wake up skip couse job is still nullptr
			// after getting job execute and complete it and reset acquire job result
			if (acquireJobResult.m_Job != nullptr)
			{
				acquireJobResult.m_Job->Execute_Internal(
					acquireJobResult.m_JobContextIndex,
					acquireJobResult.m_JobElementCount,
					acquireJobResult.m_DesiredBatchSize,
					*m_MainThreadContext
				);
				acquireJobResult.m_JobDependecy->SetCompleted();
				acquireJobResult.Reset();
			}
		}
	}

	void JobSystemManager::PerformJobsOnMainThreadUntilDepenedcyCompleted(JobDependency& dependecy)
	{
		JobThreadData acquireJobResult = {};
		uint64_t jobCount = 0;

		while (!dependecy.IsCompleted())
		{
			// get job from queue and remove from it
			acquireJobResult = m_BaseJobsQueue.GetRemoveJob(jobCount);

			// after wake up skip couse job is still nullptr
			// after getting job execute and complete it and reset acquire job result
			if (acquireJobResult.m_Job != nullptr)
			{
				acquireJobResult.m_Job->Execute_Internal(
					acquireJobResult.m_JobContextIndex,
					acquireJobResult.m_JobElementCount,
					acquireJobResult.m_DesiredBatchSize,
					*m_MainThreadContext
				);
				acquireJobResult.m_JobDependecy->SetCompleted();
				acquireJobResult.Reset();
			}
		}
	}
}
