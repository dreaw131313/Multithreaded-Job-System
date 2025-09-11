#include "JobManager.h"

namespace JobSystem
{

	struct BatchCountAndSize
	{
	public:
		int64_t BatchCount = 0;
		int64_t BatchSize = 0;

	public:
		static BatchCountAndSize CalculateBatchCountAndSize(
			int64_t elementCount,
			int64_t minBatchSize,
			int64_t maxBatchCount
		)
		{
			uint32_t batchCountWithMinBatchSize = static_cast<uint32_t>(elementCount / minBatchSize);
			if ((batchCountWithMinBatchSize * minBatchSize) < elementCount)
			{
				batchCountWithMinBatchSize += 1;
			}
			if (batchCountWithMinBatchSize <= maxBatchCount)
			{
				return { batchCountWithMinBatchSize, minBatchSize };
			}

			int64_t contextCount = maxBatchCount;
			int64_t desiredBatchSize = elementCount / contextCount;
			int64_t checkedElementsCount = desiredBatchSize * contextCount;

			if (checkedElementsCount < elementCount)
			{
				desiredBatchSize += 1;
			}

			return { maxBatchCount, desiredBatchSize };
		}
	};

	JobManager::JobManager()
	{
		Initialize({});
	}

	JobManager::JobManager(const JobManagerConfig& configuration)
	{
		Initialize(configuration);
	}

	JobManager::~JobManager()
	{
		Destroy();
	}

	void JobManager::CompleteJobs()
	{
		while (m_BaseJobsQueue.GetJobsCount() > 0)
		{
			PerformJobsOnMainThread();
		}

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

	void JobManager::WakeupThreads(int32_t threadsToWakeUp)
	{
		int32_t maxThreads = std::max(threadsToWakeUp, m_WorkerThreadsCount);
		for (int32_t i = 0; i < maxThreads; i++)
		{
			m_WorkerThreadContexts[m_LastWakedThreadIndex]->WakeUp();
			m_LastWakedThreadIndex = (m_LastWakedThreadIndex + 1) % m_WorkerThreadsCount;
		}
	}

	JobDependency JobManager::Schedule(
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
			TRefCounterHandle<JobDependencyData> jobDependecyData = TRefCounterHandle<JobDependencyData>::Make((int)jobContextCount);
			m_BaseJobsQueue.QueueJob(
				job,
				jobDependecyData,
				jobContextCount,
				jobElementCount,
				desiredBatchSize,
				dependecies,
				dependecyCount
			);

			// wakeup correct number of threads:
			WakeupThreads(static_cast<int32_t>(jobContextCount));

			return JobDependency(jobDependecyData, this);
		}

		return JobDependency();
	}

	JobDependency JobManager::Schedule(Job* job, JobDependency* dependecies, uint64_t dependecyCount)
	{
		return Schedule(job, 1, -1, -1, dependecies, dependecyCount);
	}

	JobDependency JobManager::ScheduleParallelFor(JobParallelFor* job, int64_t elementCount, int64_t batchSize, JobDependency* dependecies, uint64_t dependecyCount)
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

	JobDependency JobManager::ScheduleParallelForBatch(JobParallelForBatch* job, int64_t elementCount, int64_t maxBatchSize, JobDependency* dependecies, uint64_t dependecyCount)
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

	JobDependency JobManager::ScheduleParallelForBatch2(JobParallelForBatch* job, int64_t elementCount, int64_t maxBatches, JobDependency* dependecies, uint64_t dependecyCount)
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

			if (checkedElementsCount < elementCount)
			{
				desiredBatchSize += 1;
			}

			return Schedule(job, contextCount, elementCount, desiredBatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	JobDependency JobManager::ScheduleParallelForBatch3(
		JobParallelForBatch* job,
		int64_t elementCount,
		int64_t minBatchSize,
		int64_t maxBatchCount,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (elementCount > 0 && minBatchSize > 0 && maxBatchCount > 0)
		{
			BatchCountAndSize size = BatchCountAndSize::CalculateBatchCountAndSize(elementCount, minBatchSize, maxBatchCount);

			return Schedule(job, size.BatchCount, elementCount, size.BatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	void JobManager::PerformJobsOnMainThread()
	{
		uint32_t remainingJobs = 0;
		do
		{
			remainingJobs = 0;
			for (auto jobQueue : m_MainThreadJobQueues)
			{
				if (jobQueue->CanExecuteOnMainThread())
				{
					jobQueue->ThreadLoop(*m_MainThreadContext);
				}
				remainingJobs += jobQueue->GetJobCount();
			}
		} while (remainingJobs > 0);
	}

	void JobManager::PerformJobsOnMainThreadUntilDepenedcyCompleted(JobDependency& dependecy)
	{
		while (!dependecy.IsCompleted())
		{
			for (auto jobQueue : m_MainThreadJobQueues)
			{
				if (jobQueue->CanExecuteOnMainThread())
				{
					jobQueue->ThreadLoopOnMainThread(*m_MainThreadContext);

					if (dependecy.IsCompleted())
					{
						break;
					}
				}
			}
		}
	}

	void JobManager::Initialize(const JobManagerConfig& configuration)
	{
		Destroy();

		if (m_bIsInitialized)
		{
			return;
		}
		m_bIsInitialized = true;

		int workerThreadCount = std::thread::hardware_concurrency() - 2;
		if (workerThreadCount <= 0)
		{
			return;
		}

		if (configuration.m_WorkerThreadCount > 0)
		{
			m_WorkerThreadsCount = std::min(workerThreadCount, configuration.m_WorkerThreadCount);
		}
		else
		{
			m_WorkerThreadsCount = workerThreadCount;
		}

		// setup job queues
		{
			if (!configuration.m_JobQueues.empty())
			{
				m_JobQueues = configuration.m_JobQueues;
				for (auto queue : m_JobQueues)
				{
					if (queue->CanExecuteOnMainThread())
					{
						m_MainThreadJobQueues.push_back(queue);
					}
				}
			}

			m_JobQueues.push_back(&m_BaseJobsQueue);
			m_MainThreadJobQueues.push_back(&m_BaseJobsQueue);
		}


		auto threadLoop = [this](ThreadContext* context)
		{
			if (context == nullptr)
			{
				return;
			}

			context->m_ThreadID = std::hash<std::thread::id>()(std::this_thread::get_id());

			while (context->IsAlive())
			{
				for (auto jobQueue : m_JobQueues)
				{
					jobQueue->ThreadLoop(*context);
				}

				context->Sleep();
			}
		};

		m_MainThreadContext = std::make_unique<ThreadContext>(0);
		for (int32_t i = 1; i <= m_WorkerThreadsCount; i++)
		{
			auto context = new ThreadContext(i);
			m_WorkerThreadContexts.push_back(context);

			auto& thread = m_WorkerThreads.emplace_back(threadLoop, context);
		}

	}

	void JobManager::Destroy()
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

		m_MainThreadContext.reset();

		// clearing containers:
		m_WorkerThreadContexts.clear();
		m_WorkerThreads.clear();

		// clearing queue:
		m_BaseJobsQueue.Clear();
	}

}