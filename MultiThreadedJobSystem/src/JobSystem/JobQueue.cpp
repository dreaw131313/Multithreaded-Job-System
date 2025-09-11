#include "JobQueue.h"

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

	void JobQueue::Clear()
	{
		std::scoped_lock lock(m_Mutex);
		m_JobCount = 0;

		m_Front = nullptr;
		m_Back = nullptr;

		m_NodeAllocator.clear();
		m_FreeNodes.clear();
	}

	JobDependency JobQueue::Schedule(
		JobManager& parentJobManager,
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
			QueueJob(
				job,
				jobDependecyData,
				jobContextCount,
				jobElementCount,
				desiredBatchSize,
				dependecies,
				dependecyCount
			);

			// wakeup correct number of threads:
			parentJobManager.WakeupThreads(static_cast<int32_t>(jobContextCount));

			return JobDependency(jobDependecyData, parentJobManager);
		}

		return JobDependency();
	}

	JobDependency JobQueue::Schedule(
		JobManager& parentJobManager,
		Job* job,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		return Schedule(parentJobManager, job, 1, -1, -1, dependecies, dependecyCount);
	}

	JobDependency JobQueue::ScheduleParallelFor(
		JobManager& parentJobManager,
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

			return Schedule(parentJobManager, job, contextCount, elementCount, batchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	JobDependency JobQueue::ScheduleParallelForBatch(
		JobManager& parentJobManager,
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
				return Schedule(parentJobManager, job, 1, elementCount, elementCount, dependecies, dependecyCount);
			}

			int64_t contextCount = elementCount / maxBatchSize;
			int64_t modulo = elementCount % maxBatchSize;
			if (modulo > 0)
			{
				contextCount += 1;
			}

			return Schedule(parentJobManager, job, contextCount, elementCount, maxBatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	JobDependency JobQueue::ScheduleParallelForBatch2(
		JobManager& parentJobManager,
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
				return Schedule(parentJobManager, job, 1, elementCount, elementCount, dependecies, dependecyCount);
			}
			else if (elementCount == maxBatches)
			{
				return Schedule(parentJobManager, job, maxBatches, elementCount, 1, dependecies, dependecyCount);
			}

			int64_t contextCount = maxBatches;
			int64_t desiredBatchSize = elementCount / contextCount;
			int64_t checkedElementsCount = desiredBatchSize * contextCount;

			if (checkedElementsCount < elementCount)
			{
				desiredBatchSize += 1;
			}

			return Schedule(parentJobManager, job, contextCount, elementCount, desiredBatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	JobDependency JobQueue::ScheduleParallelForBatch3(
		JobManager& parentJobManager,
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

			return Schedule(parentJobManager, job, size.BatchCount, elementCount, size.BatchSize, dependecies, dependecyCount);
		}

		return JobDependency();
	}

	void JobQueue::ThreadLoop(const ThreadContext& threadContext)
	{
		JobDequeueResult dequeueJobResult = {};
		uint64_t remainingJobs = 0;
		do
		{
			if (DequeueJob(dequeueJobResult, remainingJobs))
			{
				dequeueJobResult.m_Job->Execute_Internal(
					dequeueJobResult.m_JobContextIndex,
					dequeueJobResult.m_JobElementCount,
					dequeueJobResult.m_DesiredBatchSize,
					threadContext
				);
				dequeueJobResult.m_JobDependecy->SetCompleted();
			}
		} while (remainingJobs > 0);

	}

	void JobQueue::ThreadLoopOnMainThread(const ThreadContext& threadContext)
	{
		JobDequeueResult dequeueJobResult = {};
		if (DequeueJob(dequeueJobResult))
		{
			dequeueJobResult.m_Job->Execute_Internal(
				dequeueJobResult.m_JobContextIndex,
				dequeueJobResult.m_JobElementCount,
				dequeueJobResult.m_DesiredBatchSize,
				threadContext
			);
			dequeueJobResult.m_JobDependecy->SetCompleted();
		}
	}

	bool JobQueue::QueueJob(
		JobBase* job,
		const TRefCounterHandle<JobDependencyData>& jobDependecyData,
		int64_t jobContextCount,
		int64_t jobElementCount,
		int64_t desiredBatchSize,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (job == nullptr)
		{
			return false;
		}

		std::scoped_lock lock(m_Mutex);

		Node* newNode = CreateNode(
			job,
			jobDependecyData,
			jobContextCount,
			jobElementCount,
			desiredBatchSize,
			dependecies,
			dependecyCount
		);

		if (m_Back != nullptr)
		{
			newNode->m_Previous = m_Back;
			m_Back->m_Next = newNode;

			m_Back = newNode;
		}
		else
		{
			m_Front = newNode;
			m_Back = newNode;
		}

		m_JobCount++;

		return true;
	}

	JobQueue::Node* JobQueue::CreateNode(
		JobBase* job,
		const TRefCounterHandle<JobDependencyData>& jobDependecyData,
		int64_t jobContextCount,
		int64_t jobElementCount,
		int64_t desiredBatchSize,
		JobDependency* dependecies,
		uint64_t dependecyCount
	)
	{
		if (!m_FreeNodes.empty())
		{
			auto node = m_FreeNodes.back();
			m_FreeNodes.pop_back();
			node->m_JobData.Init(
				job,
				jobDependecyData,
				jobContextCount,
				jobElementCount,
				desiredBatchSize,
				dependecies,
				dependecyCount
			);

			return node;
		}

		return &m_NodeAllocator.emplace_back(
			job,
			jobDependecyData,
			jobContextCount,
			jobElementCount,
			desiredBatchSize,
			dependecies,
			dependecyCount
		);
	}

	JobQueue::Node* JobQueue::GetFirstNodeWithJobToStart()
	{
		Node* currentNode = m_Front;
		while (currentNode != nullptr && !currentNode->m_JobData.IsValidToStart())
		{
			currentNode = currentNode->m_Next;
		}

		return currentNode;
	}

	void JobQueue::EraseNodeFromLinkedList(Node* node)
	{
		Node* previous = node->m_Previous;
		Node* next = node->m_Next;

		node->m_Previous = nullptr;
		node->m_Next = nullptr;

		if (previous != nullptr)
		{
			previous->m_Next = next;
		}

		if (next != nullptr)
		{
			next->m_Previous = previous;
		}

		if (m_Front == node)
		{
			m_Front = next;
		}

		if (m_Back == node)
		{
			m_Back = previous;
		}
	}

	bool JobQueue::DequeueJob_Internal(JobDequeueResult& result)
	{
		if (m_JobCount > 0)
		{
			Node* nodeWithJob = GetFirstNodeWithJobToStart();
			if (nodeWithJob != nullptr)
			{
				JobQueueData& jobData = nodeWithJob->m_JobData;
				// Fill result:
				{
					result.m_JobDependecy = jobData.m_JobDependecyData;
					result.m_Job = jobData.m_Job;
					result.m_JobContextIndex = jobData.m_CurrentJobContext;
					result.m_JobElementCount = jobData.m_JobElementCount;
					result.m_DesiredBatchSize = jobData.m_DesiredBatchSize;
				}

				jobData.m_CurrentJobContext++;
				if (jobData.m_CurrentJobContext >= jobData.m_JobContextCount)
				{
					EraseNodeFromLinkedList(nodeWithJob);
					DestroyNode(nodeWithJob);
					m_JobCount--;
				}

				return true;
			}
		}

		return false;
	}

	void JobQueueWithThreadCountLimit::ThreadLoop(const ThreadContext& threadContext)
	{
		if (m_CurrentWorkThreadCount.load() < m_MaxSimultaneousThreadCount.load())
		{
			m_CurrentWorkThreadCount.fetch_add(1);
			{
				JobQueue::ThreadLoop(threadContext);
			}
			m_CurrentWorkThreadCount.fetch_sub(1);
		}
	}

	void JobQueueWithThreadCountLimit::ThreadLoopOnMainThread(const ThreadContext& threadContext)
	{
		if (m_CurrentWorkThreadCount.load() < m_MaxSimultaneousThreadCount.load())
		{
			m_CurrentWorkThreadCount.fetch_add(1);
			{
				JobQueue::ThreadLoopOnMainThread(threadContext);
			}
			m_CurrentWorkThreadCount.fetch_sub(1);
		}
	}

}
