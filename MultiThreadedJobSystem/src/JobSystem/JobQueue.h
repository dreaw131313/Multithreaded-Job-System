#pragma once
#pragma once

#include <deque>

#include "JobManagerQueue.h"
#include "Job.h"
#include "JobDependency.h"


namespace djs
{
	class JobManager;

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

	class JobQueueData
	{
		friend class JobQueue;
	public:
		std::vector<JobDependency> m_Dependecies = {};
		TRefCounterHandle<JobDependencyData> m_JobDependecyData = {};
		JobBase* m_Job = nullptr;
		int64_t m_JobContextCount = -1;
		int64_t m_CurrentJobContext = 0;

		int64_t m_JobElementCount = -1;
		int64_t m_DesiredBatchSize = -1;

	public:
		JobQueueData() = default;

		JobQueueData(
			JobBase* job,
			const TRefCounterHandle<JobDependencyData>& jobDependecyData,
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

		bool IsValidToStart()
		{
			while (!m_Dependecies.empty())
			{
				auto& dependecy = m_Dependecies.back();
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
			const TRefCounterHandle<JobDependencyData>& jobDependecyData,
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
			m_JobDependecyData.Reset();
			m_Job = nullptr;
			m_JobContextCount = -1;
			m_CurrentJobContext = 0;
			m_JobElementCount = -1;
			m_DesiredBatchSize = -1;
		}

	};

	class JobDequeueResult
	{
	public:
		TRefCounterHandle<JobDependencyData> m_JobDependecy = {};
		JobBase* m_Job = nullptr;
		int64_t m_JobContextIndex = 0;
		int64_t m_JobElementCount = 0; // this is how much elements is passed to job, need for Parallel and PrallelBatch jobs, it indicates number of all elements which should be splited between all job contexts
		int64_t m_DesiredBatchSize = -1;

	public:
		JobDequeueResult()
		{

		}

		JobDequeueResult(
			const TRefCounterHandle<JobDependencyData>& jobDependecy,
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
			m_JobDependecy.Reset();
			m_Job = nullptr;
			m_JobContextIndex = std::numeric_limits<uint32_t>::max();
		}
	};

	class JobQueue : public JobManagerQueue
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
				const TRefCounterHandle<JobDependencyData>& jobDependecyData,
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

		JobDependency Schedule(
			JobManager& parentJobManager,
			JobBase* job,
			int64_t jobContextCount = 1,
			int64_t jobElementCount = 0,
			int64_t desiredBatchSize = 0,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency Schedule(
			JobManager& parentJobManager,
			Job* job,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelFor(
			JobManager& parentJobManager,
			JobParallelFor* job,
			int64_t elementCount,
			int64_t batchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelForBatch(
			JobManager& parentJobManager,
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t maxBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelForBatch2(
			JobManager& parentJobManager,
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t maxBatches,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		JobDependency ScheduleParallelForBatch3(
			JobManager& parentJobManager,
			JobParallelForBatch* job,
			int64_t elementCount,
			int64_t minBatchSize,
			int64_t maxBatchCount,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		void Clear();

		uint32_t GetJobCount() const noexcept override
		{
			std::scoped_lock lock(m_Mutex);
			return m_JobCount;
		}

		void ThreadLoop(const ThreadContext& threadContext) override;

		inline bool CanExecuteOnMainThread() const noexcept override
		{
			return true;
		}

		void ThreadLoopOnMainThread(const ThreadContext& threadContext) override;

	protected:
		bool DequeueJob(JobDequeueResult& result)
		{
			std::scoped_lock lock(m_Mutex);
			return DequeueJob_Internal(result);
		}

		bool DequeueJob(JobDequeueResult& result, uint64_t& jobCount)
		{
			std::scoped_lock lock(m_Mutex);
			jobCount = m_JobCount;
			return DequeueJob_Internal(result);
		}

		bool QueueJob(
			JobBase* job,
			const TRefCounterHandle<JobDependencyData>& jobDependecyData,
			int64_t jobContextCount,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

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
			const TRefCounterHandle<JobDependencyData>& jobDependecyData,
			int64_t jobContextCount,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies,
			uint64_t dependecyCount
		);

		void DestroyNode(Node* node)
		{
			node->Reset();
			m_FreeNodes.push_back(node);
		}

		Node* GetFirstNodeWithJobToStart();

		void EraseNodeFromLinkedList(Node* node);

		bool DequeueJob_Internal(JobDequeueResult& result);

	};


	/// <summary>
	/// Queue whith max number of threads which can simultaneously perform jobs from this queue.
	/// </summary>
	class JobQueueWithThreadCountLimit : public JobQueue
	{
	public:
		bool SetMaxSimultaneousThreadCount(uint32_t threadCount)
		{
			if (threadCount > 0)
			{
				m_MaxSimultaneousThreadCount.store(threadCount);
				return true;
			}
			return false;
		}

		void ThreadLoop(const ThreadContext& threadContext) override;

		void ThreadLoopOnMainThread(const ThreadContext& threadContext) override;

	public:
		std::atomic<uint32_t> m_MaxSimultaneousThreadCount = std::numeric_limits<uint32_t>::max();
		std::atomic<uint32_t> m_CurrentWorkThreadCount = 0;
	};
}
