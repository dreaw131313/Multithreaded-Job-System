#pragma once
#include "Core.h"

namespace JobSystem
{
	class ThreadContext
	{
		friend class JobSystemManager;
	public:
		ThreadContext(uint32_t threadID) :
			m_ThreadID(threadID)
		{

		}

		inline uint32_t GetThreadID() const
		{
			return m_ThreadID;
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
		bool m_IsAlive = true;
		uint32_t m_ThreadID;

		mutable std::mutex m_Mutex = {};
		std::condition_variable m_ConditionVariable = {};
		uint32_t m_IsAwake = 0;

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