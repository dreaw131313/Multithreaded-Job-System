#include "JobDependency.h"

#include "JobManager.h"

namespace JobSystem
{
	JobDependency::JobDependency(std::shared_ptr<JobDependencyData>& dependcyData, JobManager* jobManager) :
		m_DependencyData(dependcyData),
		m_JobManager(jobManager)
	{
	}

	void JobDependency::Complete()
	{
		if (m_JobManager != nullptr)
		{
			m_JobManager->PerformJobsOnMainThreadUntilDepenedcyCompleted(*this);
		}
	}
}