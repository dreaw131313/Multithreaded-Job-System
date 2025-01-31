#pragma once
#include "Core.h"

namespace JobSystem
{
	class ThreadContext
	{
		friend class JobManager;
	public:
		ThreadContext(uint32_t threadIndex) :
			m_ThreadIndex(threadIndex)
		{

		}

		inline uint32_t GetThreadIndex() const
		{
			return m_ThreadIndex;
		}

		inline uint32_t GetThreadID() const
		{
			return m_ThreadIndex;
		}

		inline bool IsAlive() const
		{
			std::unique_lock lock(m_Mutex);
			return m_IsAlive;
		}

		inline bool IsAwake()
		{
			std::unique_lock lock(m_Mutex);
			return m_IsAwake > 0;
		}

	private:

		mutable std::mutex m_Mutex = {};
		std::condition_variable m_ConditionVariable = {};
		uint64_t m_ThreadID = 0;
		uint32_t m_IsAwake = 0;
		uint32_t m_ThreadIndex;

		bool m_IsAlive = true;
	private:
		inline void Kill()
		{
			std::unique_lock lock(m_Mutex);
			m_IsAlive = false;
		}

		inline void WakeUp()
		{
			std::unique_lock lock(m_Mutex);
			m_IsAwake += 1;
			m_ConditionVariable.notify_one();
		}

		inline void Sleep()
		{
			std::unique_lock lock(m_Mutex);
			while (m_IsAwake == 0)
			{
				m_ConditionVariable.wait(lock);
			}
			m_IsAwake -= 1;
		}
	};
}