#include <cstdint>
#include <iostream>
#include <vector>
#include <string>

#include <unordered_map>

#include <list>
#include <chrono>

#include "Utils/MeasureTimer.h"

#include "JobSystem/JobSystem.h"

using namespace std::chrono_literals;
using namespace JobSystem;

class TestJob : public JobSystem::JobParallelForBatch
{
public:
	virtual void Execute(int64_t batchIndex, int64_t startIndex, int64_t count, const ThreadContext& threadContext) override
	{
		int64_t value = values[batchIndex];
		for (int64_t i = 0; i < count; i++)
		{
			value += value + 1 % 8;
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(2));
		//std::cout << "Value: " << value << std::endl;

		values[batchIndex] = value;
	}

public:
	std::mutex mutex;
	std::vector<int64_t> values;
};


int main(int argc, char** args)
{
	JobSystemManager jobSystemManager = {};
	jobSystemManager.Initialize();

	int64_t elementsCount = 100000000;

	int threadsToUse = 16;
	TestJob multiThreadJob = {};
	multiThreadJob.values.resize(threadsToUse);

	TestJob singleThreadJob = {};
	singleThreadJob.values.resize(1);

	int schedules = 1000;

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	MeasureTimer timer(true);
	{
		for (int i = 0; i < schedules; i++)
		{
			auto dependecy = jobSystemManager.ScheduleParallelForBatch2(&multiThreadJob, elementsCount, threadsToUse);
			dependecy.Complete();
		}
	}
	auto multiThreadDelta = timer.ElapsedAsMilisecond();

	timer.Start();
	{
		for (int i = 0; i < schedules; i++)
		{
			auto dependecy = jobSystemManager.ScheduleParallelForBatch2(&singleThreadJob, elementsCount, 1);
			dependecy.Complete();
		}
	}
	auto singleThreadDelta = timer.ElapsedAsMilisecond();


	int64_t value = 0;
	for (int i = 0; i < threadsToUse; i++)
	{
		value += multiThreadJob.values[i];
	}

	std::cout << "Multi thread schedule avarage time: " << multiThreadDelta / schedules << " ms" << std::endl;
	std::cout << "Single thread schedules avarage time: " << singleThreadDelta / schedules << " ms" << std::endl;

	std::cout << "Final value multi thread = " << value << std::endl;
	std::cout << "Final value single thread = " << singleThreadJob.values[0] << std::endl;

	//auto dependecy = jobSystemManager.Schedule(&job, 20);
	//dependecy.Complete();

	jobSystemManager.CompleteJobs();
	jobSystemManager.Destroy();

	return 0;
}