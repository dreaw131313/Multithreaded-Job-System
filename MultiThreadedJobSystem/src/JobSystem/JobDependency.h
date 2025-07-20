#pragma once
#include "Core.h"

namespace JobSystem
{
	class JobDependencyData
	{
	public:
		JobDependencyData(int jobContextCount = 1):
			m_ContextCount(jobContextCount)
		{

		}

		inline int GetContextCount()
		{
			return m_ContextCount.load();
		}

		inline void SetCompleted()
		{
			m_ContextCount.fetch_sub(1);
		}

		inline bool IsCompleted()
		{
			return m_ContextCount.load() <= 0;
		}

	private:
		std::atomic<int> m_ContextCount = 1;
	};

	class JobDependency
	{
		friend class JobManager;

	private:
		JobDependency(std::shared_ptr<JobDependencyData>& dependcyData, JobManager* jobManager);

	public:
		JobDependency()
		{

		}
		~JobDependency()
		{
			m_DependencyData.reset();
		}

		inline bool IsCompleted()
		{
			if (m_DependencyData)
			{
				return m_DependencyData->IsCompleted();
			}
			return true;
		}

		void Complete();

		/// <summary>
		/// This waits for job to complete but will not perform jobs on waiting thread.
		/// </summary>
		inline void CompleteWithoutPerformingJobs()
		{
			if (m_DependencyData)
			{
				while (!IsCompleted());
			}
		}

	private:
		std::shared_ptr<JobDependencyData> m_DependencyData;
		JobManager* m_JobManager = nullptr;
	};
}