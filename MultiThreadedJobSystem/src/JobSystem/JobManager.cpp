#include "JobManager.h"

namespace JobSystem
{

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
		PerformJobsOnMainThread();
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