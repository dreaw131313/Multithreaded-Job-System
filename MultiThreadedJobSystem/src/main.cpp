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
			value += 1;
		}
		values[batchIndex] = value;
	}

public:
	std::mutex mutex;
	std::vector<int64_t> values;
};


int main(int argc, char** args)
{
	JobSystem::JobManagerConfig config{};
	JobManager jobSystem(config);

	int64_t elementsCount = 1000000;

	int threadsToUse = 10;
	TestJob multiThreadJob = {};
	multiThreadJob.values.resize(threadsToUse);

	int schedules = 100;

	MeasureTimer timer(true);
	{
		for (int i = 0; i < schedules; i++)
		{
			auto dependecy = jobSystem.ScheduleParallelForBatch2(&multiThreadJob, elementsCount, threadsToUse);
			dependecy.Complete();
		}
	}
	auto multiThreadDelta = timer.ElapsedAsMilisecond();

	int64_t value = 0;
	for (int i = 0; i < threadsToUse; i++)
	{
		value += multiThreadJob.values[i];
	}

	std::cout << "Multi thread schedule avarage time: " << multiThreadDelta / schedules << " ms" << std::endl;
	std::cout << "Final value multi thread = " << value << std::endl;

	/*

	TestJob singleThreadJob = {};
	singleThreadJob.values.resize(1);

	timer.Start();
	{
		for (int i = 0; i < schedules; i++)
		{
			auto dependecy = jobSystem.ScheduleParallelForBatch2(&singleThreadJob, elementsCount, 1);
			dependecy.Complete();
		}
	}
	auto singleThreadDelta = timer.ElapsedAsMilisecond();

	std::cout << "Final value single thread = " << singleThreadJob.values[0] << std::endl;
	std::cout << "Single thread schedules avarage time: " << singleThreadDelta / schedules << " ms" << std::endl;
	*/

	//auto dependecy = jobSystemManager.Schedule(&job, 20);
	//dependecy.Complete();

	jobSystem.CompleteJobs();
	jobSystem.Destroy();

	return 0;
}