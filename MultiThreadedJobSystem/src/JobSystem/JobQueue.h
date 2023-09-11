#pragma once
#include <vector>
#include <list>
#include <mutex>

#include "Job.h"
#include "JobDependency.h"

namespace JobSystem
{
	class JobQueueRecord
	{
		friend class JobQueue;
	public:
		JobQueueRecord(
			JobBase* job,
			std::shared_ptr<JobDependencyData>& jobDependecyData,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		) :
			m_Job(job),
			m_JobDependecyData(jobDependecyData),
			m_JobContextCount(jobDependecyData->GetContextCount()),
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
			if (size > 0)
			{
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
			}
			return true;
		}

	private:
		std::shared_ptr<JobDependencyData> m_JobDependecyData = {};
		std::vector<JobDependency> m_Dependecies = {};
		JobBase* m_Job = nullptr;
		int64_t m_JobContextCount = -1;
		int64_t m_CurrentJobContext = 0;

		int64_t m_JobElementCount = -1;
		int64_t m_DesiredBatchSize = -1;
	};

	class JobQueue
	{
	public:
		class JobRecordData
		{
		public:
			JobRecordData()
			{

			}

			JobRecordData(
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

			std::shared_ptr<JobDependencyData> m_JobDependecy = {};
			JobBase* m_Job = nullptr;
			int64_t m_JobContextIndex = 0;
			int64_t m_JobElementCount = 0; // this is how much elements is passed to job, need for Parallel and PrallelBatch jobs, it indicates number of all elements which should be splited between all job contexts
			int64_t m_DesiredBatchSize = -1;

			inline void Reset()
			{
				m_JobDependecy.reset();
				m_Job = nullptr;
				m_JobContextIndex = std::numeric_limits<uint32_t>::max();
			}
		};

	public:
		JobQueue();

		~JobQueue();

		JobRecordData GetRemoveJob();

		bool AddJob(
			JobBase* job,
			std::shared_ptr<JobDependencyData>& jobDependecyData,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			JobDependency* dependecies = nullptr,
			uint64_t dependecyCount = 0
		);

		void Clear();

		inline uint32_t GetJobsCount()
		{
			std::unique_lock lock(m_Mutex);
			return m_JobCount;
		}

	private:
		uint32_t m_JobCount = 0;
		std::list<JobQueueRecord> m_JobsRecord = {};

		mutable std::mutex m_Mutex;
	};
}