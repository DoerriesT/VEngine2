#include "gtest/gtest.h"
#include "job/JobSystem.h"
#include <random>
#include <EASTL/atomic.h>

TEST(Task, testRunAndWait)
{
	job::init();

	std::default_random_engine e;
	std::uniform_int_distribution<uint64_t> d(1, 100);
	for (int i = 0; i < 40000; i++) {
		const uint64_t initialValue = d(e);
		const uint32_t incrementLoops = 50;

		struct Data
		{
			eastl::atomic<uint64_t> value;
			int iVal;
			job::Counter *counter;
		};

		Data data{};
		data.value = initialValue;
		data.iVal = i;

		for (uint32_t i2 = 0; i2 < incrementLoops; i2++)
		{
			auto t = job::Job([](void *dataV)
				{
					Data *data = (Data *)dataV;
					data->value.fetch_add(1);
				}, &data);

			job::run(1, &t, &data.counter);
		}
		job::waitForCounter(data.counter, true);
		job::freeCounter(data.counter);

		auto val = data.value.load();
		if (val != (initialValue + incrementLoops))
		{
			printf("FAIL\n");
		}
		ASSERT_EQ(val, (initialValue + incrementLoops));
	}

	job::shutdown();
}