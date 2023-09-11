#pragma once
#include <cstdint>
#include "JobsThreadContext.h"

namespace JobSystem
{
	class JobBase
	{
		friend class JobQueue;
		friend class JobSystemManager;
	public:
		JobBase();

		~JobBase();

		virtual void Execute(
			int64_t jobContextIndex,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			const JobsThreadContext& threadContext
		) = 0;
	};

	class Job : public JobBase
	{
	public:
		virtual void Execute() = 0;

	private:
		virtual void Execute(
			int64_t jobContextIndex,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			const JobsThreadContext& threadContext
		) final
		{
			Execute();
		}
	};

	class JobParallelFor : public JobBase
	{
		friend class JobSystemManager;
	public:
		virtual void Execute(int64_t index) = 0;

	private:
		virtual void Execute(
			int64_t jobContextIndex,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			const JobsThreadContext& threadContext
		) final
		{
			if (jobElementCount <= 0 || desiredBatchSize <= 0)
			{
				return;
			}

			int64_t startIndex = jobContextIndex * desiredBatchSize;
			int64_t distanceToEnd = jobElementCount - startIndex;
			if (distanceToEnd < 0)
			{
				return;
			}

			int64_t count = distanceToEnd >= desiredBatchSize ? desiredBatchSize : distanceToEnd;

			for (int64_t index = 0; index < count; index++)
			{
				Execute(index);
			}
		}
	};

	class JobParallelForBatch : public JobBase
	{
	public:
		virtual void Execute(int64_t batchIndex, int64_t startIndex, int64_t count) = 0;

	private:
		virtual void Execute(
			int64_t jobContextIndex,
			int64_t jobElementCount,
			int64_t desiredBatchSize,
			const JobsThreadContext& threadContext
		) final
		{
			if (jobElementCount <= 0 || desiredBatchSize <= 0)
			{
				return;
			}

			int64_t startIndex = jobContextIndex * desiredBatchSize;
			int64_t distanceToEnd = jobElementCount - startIndex;
			if (distanceToEnd < 0)
			{
				return;
			}

			int64_t count = distanceToEnd >= desiredBatchSize ? desiredBatchSize : distanceToEnd;

			Execute(jobContextIndex, startIndex, count);
		}
	};

}
