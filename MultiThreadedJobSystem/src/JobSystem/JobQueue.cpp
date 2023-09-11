
#include "JobQueue.h"

namespace JobSystem
{
	JobQueue::JobQueue()
	{
	}

	JobQueue::~JobQueue()
	{
	}

	JobQueue::JobRecordData JobQueue::GetRemoveJob()
	{
		std::unique_lock lock(m_Mutex);
		if (m_JobCount > 0)
		{
			for (auto it = m_JobsRecord.begin(); it != m_JobsRecord.end(); it++)
			{
				JobQueueRecord& record = *it;

				if (record.IsValidToStart())
				{
					JobQueue::JobRecordData recordData(
						record.m_JobDependecyData, 
						record.m_Job, 
						(uint32_t)record.m_CurrentJobContext, 
						record.m_JobElementCount,
						record.m_DesiredBatchSize
					);
					record.m_CurrentJobContext += 1;
					if (record.m_CurrentJobContext >= record.m_JobContextCount)
					{
						m_JobsRecord.erase(it);
						m_JobCount -= 1;
					}
					return recordData;
				}
			}
		}

		return JobQueue::JobRecordData();
	}

	bool JobQueue::AddJob(
		JobBase* job,
		std::shared_ptr<JobDependencyData>& jobDependecyData,
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

		std::unique_lock lock(m_Mutex);
		m_JobsRecord.emplace_back(job, jobDependecyData, jobElementCount, desiredBatchSize, dependecies, dependecyCount);
		m_JobCount += 1;

		return true;
	}

	void JobQueue::Clear()
	{
		std::unique_lock lock(m_Mutex);
		m_JobsRecord.clear();
		m_JobCount = 0;
	}
}