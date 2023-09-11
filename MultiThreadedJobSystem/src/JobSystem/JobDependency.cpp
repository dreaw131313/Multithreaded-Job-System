#include "JobDependency.h"

namespace JobSystem
{
	JobDependency::JobDependency()
	{
	}

	JobDependency::JobDependency(std::shared_ptr<JobDependencyData>& dependcyData):
		m_DependencyData(dependcyData)
	{
	}

}