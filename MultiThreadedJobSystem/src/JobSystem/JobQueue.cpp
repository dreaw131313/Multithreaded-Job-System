#include "JobQueue.h"


namespace JobSystem
{
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

	void JobQueue::Clear()
	{
		std::scoped_lock lock(m_Mutex);
		m_JobCount = 0;

		m_Front = nullptr;
		m_Back = nullptr;

		m_NodeAllocator.clear();
		m_FreeNodes.clear();
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

	JobQueue::Node* JobQueue::CreateNode(JobBase* job, const TRefCounterHandle<JobDependencyData>& jobDependecyData, int64_t jobContextCount, int64_t jobElementCount, int64_t desiredBatchSize, JobDependency* dependecies, uint64_t dependecyCount)
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

}
