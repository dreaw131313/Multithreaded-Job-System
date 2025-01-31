#pragma once
#include "Job.h"
#include "JobDependency.h"

#include <deque>

namespace JobSystem
{
	class JobQueueData
	{
		friend class JobQueue;
	public:
		std::vector<JobDependency> m_Dependecies = {};
		std::shared_ptr<JobDependencyData> m_JobDependecyData = {};
		JobBase* m_Job = nullptr;
		int64_t m_JobContextCount = -1;
		int64_t m_CurrentJobContext = 0;

		int64_t m_JobElementCount = -1;
		int64_t m_DesiredBatchSize = -1;

	public:
		JobQueueData() = default;

		JobQueueData(
			JobBase* job,
			std::shared_ptr<JobDependencyData>& jobDependecyData,
			int64_t jobContextCount,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies,
			uint64_t dependecyCount
		):
			m_JobDependecyData(jobDependecyData),
			m_Job(job),
			m_JobContextCount(jobContextCount),
			m_JobElementCount(jobElementCount),
			m_DesiredBatchSize(desiredBatchSize)
		{
			if (dependecyCount > 0)
			{
				m_Dependecies.insert(m_Dependecies.begin(), dependecies, dependecies + dependecyCount);
			}
		}


		inline bool IsValidToStart()
		{
			int64_t size = (int64_t)m_Dependecies.size();

			for (int64_t i = size - 1; i >= 0; i--)
			{
				auto& dependecy = m_Dependecies[i];
				if (dependecy.IsCompleted())
				{
					m_Dependecies.pop_back();
				}
				else
				{
					return false;
				}
			}
			return true;
		}

		void Init(
			JobBase* job,
			std::shared_ptr<JobDependencyData>& jobDependecyData,
			int64_t jobContextCount,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies,
			uint64_t dependecyCount
		)
		{
			m_JobDependecyData = jobDependecyData;
			m_Job = job;
			m_JobContextCount = jobContextCount;
			m_JobElementCount = jobElementCount;
			m_DesiredBatchSize = desiredBatchSize;

			m_Dependecies.insert(m_Dependecies.begin(), dependecies, dependecies + dependecyCount);
		}

		void Reset()
		{
			m_Dependecies.clear();
			m_JobDependecyData.reset();
			m_Job = nullptr;
			m_JobContextCount = -1;
			m_CurrentJobContext = 0;
			m_JobElementCount = -1;
			m_DesiredBatchSize = -1;
		}

	};

	class JobThreadData
	{
	public:
		std::shared_ptr<JobDependencyData> m_JobDependecy = {};
		JobBase* m_Job = nullptr;
		int64_t m_JobContextIndex = 0;
		int64_t m_JobElementCount = 0; // this is how much elements is passed to job, need for Parallel and PrallelBatch jobs, it indicates number of all elements which should be splited between all job contexts
		int64_t m_DesiredBatchSize = -1;

	public:
		JobThreadData()
		{

		}

		JobThreadData(
			const std::shared_ptr<JobDependencyData>& jobDependecy,
			JobBase* job,
			int64_t jobContextIndex,
			int64_t jobElementCount,
			int64_t desiredBatchSize
		):
			m_JobDependecy(jobDependecy),
			m_Job(job),
			m_JobContextIndex(jobContextIndex),
			m_JobElementCount(jobElementCount),
			m_DesiredBatchSize(desiredBatchSize)
		{

		}

		inline void Reset()
		{
			m_JobDependecy.reset();
			m_Job = nullptr;
			m_JobContextIndex = std::numeric_limits<uint32_t>::max();
		}
	};
	class JobQueue
	{
		struct Node final
		{
		public:
			JobQueueData m_JobData{};
			Node* m_Previous = nullptr;
			Node* m_Next = nullptr;

		public:
			Node(
				JobBase* job,
				std::shared_ptr<JobDependencyData>& jobDependecyData,
				int64_t jobContextCount,
				int64_t jobElementCount,
				int64_t desiredBatchSize,
				JobDependency* dependecies,
				uint64_t dependecyCount
			):
				m_JobData(
					job,
					jobDependecyData,
					jobContextCount,
					jobElementCount,
					desiredBatchSize,
					dependecies,
					dependecyCount
				)
			{

			}

			void Reset()
			{
				m_JobData.Reset();
				m_Previous = nullptr;
				m_Next = nullptr;
			}
		};

	public:
		JobQueue()
		{

		}

		~JobQueue()
		{

		}

		JobThreadData GetRemoveJob(uint64_t& jobCount)
		{
			std::unique_lock lock(m_Mutex);
			jobCount = m_JobCount;
			if (m_JobCount > 0)
			{
				Node* nodeWithJob = GetFirstNodeWithJobToStart();
				if (nodeWithJob != nullptr)
				{
					JobQueueData& jobData = nodeWithJob->m_JobData;

					JobThreadData jobThreadData(
						jobData.m_JobDependecyData,
						jobData.m_Job,
						(uint32_t)jobData.m_CurrentJobContext,
						jobData.m_JobElementCount,
						jobData.m_DesiredBatchSize
					);

					jobData.m_CurrentJobContext++;

					if (jobData.m_CurrentJobContext >= jobData.m_JobContextCount)
					{
						EraseNodeFromLinkedList(nodeWithJob);
					}
					return jobThreadData;
				}
			}

			return JobThreadData();
		}

		bool AddJob(
			JobBase* job,
			std::shared_ptr<JobDependencyData>& jobDependecyData,
			int64_t jobContextCount,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
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

		void Clear()
		{
			std::unique_lock lock(m_Mutex);
			m_JobCount = 0;

			m_Front = nullptr;
			m_Back = nullptr;

			m_NodeAllocator.clear();
			m_FreeNodes.clear();
		}

		inline uint32_t GetJobsCount() const
		{
			std::unique_lock lock(m_Mutex);
			return m_JobCount;
		}

	private:
		mutable std::mutex m_Mutex;

		std::deque<Node> m_NodeAllocator{};
		std::vector<Node*> m_FreeNodes{};

		Node* m_Front = nullptr;
		Node* m_Back = nullptr;

		uint32_t m_JobCount = 0;

	private:
		Node* CreateNode(
			JobBase* job,
			std::shared_ptr<JobDependencyData>& jobDependecyData,
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

		void DestroyNode(Node* node)
		{
			node->Reset();
			m_FreeNodes.push_back(node);
		}

		Node* GetFirstNodeWithJobToStart()
		{
			Node* currentNode = m_Front;
			while (currentNode != nullptr && !currentNode->m_JobData.IsValidToStart())
			{
				currentNode = currentNode->m_Next;
			}

			return currentNode;
		}

		void EraseNodeFromLinkedList(Node* node)
		{
			Node* previous = node->m_Previous;
			Node* next = node->m_Next;

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
				m_Front = node->m_Next;
			}

			if (m_Back == node)
			{
				m_Back = node->m_Previous;
			}

			m_JobCount--;
		}
	};
}