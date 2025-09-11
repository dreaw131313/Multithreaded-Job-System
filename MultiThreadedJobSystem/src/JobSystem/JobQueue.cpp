#include "JobQueue.h"


namespace JobSystem
{

	void JobQueue::ThreadLoop(const ThreadContext& threadContext)
	{
		JobDequeueResult dequeueJobResult = {};
		uint64_t remainingJobs = 0;
		do
		{
			if (DequeueJob(dequeueJobResult, remainingJobs))
			{
				dequeueJobResult.m_Job->Execute_Internal(
					dequeueJobResult.m_JobContextIndex,
					dequeueJobResult.m_JobElementCount,
					dequeueJobResult.m_DesiredBatchSize,
					threadContext
				);
				dequeueJobResult.m_JobDependecy->SetCompleted();
			}
		} while (remainingJobs > 0);

	}

	void JobQueue::ThreadLoopOnMainThread(const ThreadContext& threadContext)
	{
		JobDequeueResult dequeueJobResult = {};
		if (DequeueJob(dequeueJobResult))
		{
			dequeueJobResult.m_Job->Execute_Internal(
				dequeueJobResult.m_JobContextIndex,
				dequeueJobResult.m_JobElementCount,
				dequeueJobResult.m_DesiredBatchSize,
				threadContext
			);
			dequeueJobResult.m_JobDependecy->SetCompleted();
		}
	}

}
