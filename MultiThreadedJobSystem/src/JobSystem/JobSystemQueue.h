#pragma once

#include "Core.h"

#include "ThreadContext.h"

namespace JobSystem
{

	class JobSystemQueue
	{
	public:
		JobSystemQueue() = default;

		~JobSystemQueue() = default;

		virtual uint32_t GetJobCount() const noexcept = 0;

		virtual void ThreadLoop(const ThreadContext& threadContext) = 0;

		/// <summary>
		/// Thread safe getter to check if queue can be executed on main thread, during completition of dependencies. It is checkd only once in job manager constructor, then job queues are added to list of queues which can be executed in main thread.
		/// </summary>
		/// <returns></returns>
		virtual bool CanExecuteOnMainThread() const noexcept = 0;

		/// <summary>
		/// Thread loop which is executed when waiting on main thread until dependency is completed. It should perform one job at time and return to check if dependency is completed.
		/// </summary>
		/// <param name="threadContext"></param>
		virtual void ThreadLoopOnMainThread(const ThreadContext& threadContext) = 0;

	};


}