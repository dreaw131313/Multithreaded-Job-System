#pragma once
#include "Core.h"

namespace JobSystem
{
	class JobSystemManager;

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
		friend class JobSystemManager;

	private:
		JobDependency(std::shared_ptr<JobDependencyData>& dependcyData, JobSystemManager* jobSystemManager);

	public:
		JobDependency()
		{

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

		inline void CompleteWithoutPerformingJobs()
		{
			if (m_DependencyData)
			{
				while (!IsCompleted());
			}
		}

	private:
		std::shared_ptr<JobDependencyData> m_DependencyData;

		JobSystemManager* m_JobSystemManager = nullptr;
	};
}