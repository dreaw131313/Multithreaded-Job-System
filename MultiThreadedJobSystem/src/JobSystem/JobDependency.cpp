#include "JobDependency.h"

#include "JobSystemManager.h"

namespace JobSystem
{
	JobDependency::JobDependency(std::shared_ptr<JobDependencyData>& dependcyData, JobSystemManager* jobSystemManager) :
		m_DependencyData(dependcyData),
		m_JobSystemManager(jobSystemManager)
	{
	}

	void JobDependency::Complete()
	{
		if (m_JobSystemManager != nullptr)
		{
			m_JobSystemManager->PerformJobsOnMainThreadUntilDepenedcyCompleted(*this);
		}
	}
}