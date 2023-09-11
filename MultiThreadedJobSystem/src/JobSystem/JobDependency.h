#pragma once
#include <cstdint>
#include <mutex>

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
			return m_ContextCount;
		}

		inline void SetCompleted()
		{
			std::unique_lock lock(m_DataMutex);
			m_ContextCount -= 1;
		}

		inline bool IsCompleted()
		{
			std::unique_lock lock(m_DataMutex);
			return m_ContextCount <= 0;
		}

	private:
		mutable std::mutex m_DataMutex = {};
		int m_ContextCount = 1;
	};

	class JobDependency
	{
		friend class JobSystemManager;

	private:
		JobDependency(std::shared_ptr<JobDependencyData>& dependcyData);

	public:
		JobDependency();

		inline bool IsCompleted()
		{
			if (m_DependencyData)
			{
				return m_DependencyData->IsCompleted();
			}
			return true;
		}

		inline void Complete()
		{
			if (m_DependencyData)
			{
				while (!IsCompleted());
			}
		}

	private:
		std::shared_ptr<JobDependencyData> m_DependencyData;
	};
}