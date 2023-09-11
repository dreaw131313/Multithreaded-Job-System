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
		int value = 0;
		for (int64_t i = 0; i < count; i++)
		{
			value += 1;
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(2));
		//std::cout << "Value: " << value << std::endl;

		values[batchIndex] = value;
	}

public:
	std::mutex mutex;
	std::vector<int> values;
};


int main(int argc, char** args)
{
	JobSystemManager jobSystemManager = {};
	jobSystemManager.Initialize();

	TestJob job = {};
	int valueCount = 11;
	int elementsCount = 10000000;
	job.values.resize(valueCount);

	int schedules = 1000;

	MeasureTimer timer(true);
	{
		for (int i = 0; i < schedules; i++)
		{
			auto dependecy = jobSystemManager.ScheduleParallelForBatch2(&job, elementsCount, valueCount);
			dependecy.Complete();
		}
	}
	auto delta = timer.ElapsedAsMilisecond();

	std::cout << "Single schedules avarage time" << delta / schedules << " ms" << std::endl;

	int value = 0;
	for (int i = 0; i < valueCount; i++)
	{
		value += job.values[i];
		std::cout << "Value " << i << " = " << job.values[i] << std::endl;;
	}

	std::cout << "Final value = " << value << std::endl;

	//auto dependecy = jobSystemManager.Schedule(&job, 20);
	//dependecy.Complete();

	jobSystemManager.CompleteJobs();
	jobSystemManager.Destroy();

	return 0;
}