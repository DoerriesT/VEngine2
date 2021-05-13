#include "Engine.h"
#include <EASTL/fixed_vector.h>
#include "task/Task.h"
#include <stdio.h>
#include "ecs/ECS.h"

void *__cdecl operator new[](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

void *__cdecl operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char *pName, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

struct Position
{
	float m_posX;
	float m_posY;
	float m_posZ;
};

struct Scale
{
	float m_scale = 1.0f;
};

int main(int argc, char *argv[])
{
	ECS ecs;

	ecs.registerComponent<Position>();
	ecs.registerComponent<Scale>();

	EntityID a = ecs.createEntity();
	Position pos0 = { 1.0f, 2.0f, 3.0f };
	ecs.addComponent<Position>(a, pos0);

	EntityID b = ecs.createEntity();
	Position pos1 = { 3.0f, 2.0f, 3.0f };
	Scale scale1 = { 5.0f };
	ecs.addComponent<Position>(b, pos1);
	ecs.addComponent<Scale>(b, scale1);

	//ecs.iterate<Position>([](size_t count, const EntityID *entities, Position *positions)
	//	{
	//		for (size_t i = 0; i < count; ++i)
	//		{
	//			printf("Entity: %d, Position: %f %f %f\n", (int)entities[i], positions[i].m_posX, positions[i].m_posY, positions[i].m_posZ);
	//		}
	//	});

	ecs.iterate<Position, Scale>([](size_t count, const EntityID *entities, const Position *positions, Scale *scales)
		{
			for (size_t i = 0; i < count; ++i)
			{
				printf("Entity: %d, Position: %f %f %f, Scale: %f\n", (int)entities[i], positions[i].m_posX, positions[i].m_posY, positions[i].m_posZ, scales[i].m_scale);
			}
		});

	//ecs.removeComponent<Scale>(b);
	//
	//ecs.iterate<Position>([](size_t count, const EntityID *entities, Position *positions)
	//	{
	//		for (size_t i = 0; i < count; ++i)
	//		{
	//			printf("Entity: %d, Position: %f %f %f\n", (int)entities[i], positions[i].m_posX, positions[i].m_posY, positions[i].m_posZ);
	//		}
	//	});

	eastl::fixed_vector<int, 10> v;

	for (int i = 0; i < 10; ++i)
	{
		v.push_back(i);
	}

	task::init();

	auto *waitGroup = task::allocWaitGroup();
	
	task::Task tasks[256];
	for (size_t i = 0; i < 256; ++i)
	{
		tasks[i] = task::Task([](void *param)
			{
				int taskIdx = (int)(size_t)param;
				printf("Task %d says hello from thread %d and fiber %d\n", taskIdx, (int)task::getThreadIndex(), (int)task::getFiberIndex());
			}, (void *)i);
	}
	
	task::schedule(256, tasks, waitGroup, task::Priority::NORMAL);
	
	task::waitFor(waitGroup);

	task::shutdown();

	Engine engine;
	return engine.start(argc, argv);
}