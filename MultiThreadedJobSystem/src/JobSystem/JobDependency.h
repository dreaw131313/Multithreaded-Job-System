#pragma once
#include "Core.h"

#include "RefCountedObject.h"

namespace JobSystem
{

	class JobDependencyData : public RefCountedObject
	{
	public:
		JobDependencyData(int32_t jobContextCount = 1):
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
		std::atomic<int32_t> m_ContextCount = 1;
	};

	class JobDependency
	{
		friend class JobManager;
		friend class JobQueue;

	private:
		JobDependency(const TRefCounterHandle<JobDependencyData>& dependcyData, JobManager&jobManager);

	public:
		JobDependency()
		{

		}

		~JobDependency() = default;

		inline bool IsCompleted()
		{
			if (m_DependencyData.IsValid())
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
			while (!IsCompleted());
		}

		inline bool IsValid() const
		{
			return m_JobManager != nullptr && m_DependencyData.IsValid();
		}

	private:
		TRefCounterHandle<JobDependencyData> m_DependencyData{};
		JobManager* m_JobManager = nullptr;
	};
}