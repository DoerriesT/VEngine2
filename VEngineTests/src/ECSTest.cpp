#include "gtest/gtest.h"
#include "ecs/ECS.h"

struct CompA
{
	float a;
};

struct CompB
{
	float a;
	uint32_t b;
};

struct CompC
{
	float a;
	uint32_t b;
	char c;
};

template<size_t DUMMY>
struct DestructorTestComp
{
	int *destructorCallCounter = nullptr;
	
	DestructorTestComp() = default;
	
	DestructorTestComp(int *p) 
		:destructorCallCounter(p) 
	{
	};

	DestructorTestComp(const DestructorTestComp &other) = default;

	DestructorTestComp(DestructorTestComp &&other) noexcept
	{
		destructorCallCounter = other.destructorCallCounter;
		other.destructorCallCounter = nullptr;
	}

	
	DestructorTestComp &operator=(const DestructorTestComp &) = default;

	DestructorTestComp &operator=(DestructorTestComp &&other) noexcept
	{
		if (&other != this)
		{
			destructorCallCounter = other.destructorCallCounter;
			other.destructorCallCounter = nullptr;
		}

		return *this;
	}

	~DestructorTestComp()
	{
		if (destructorCallCounter)
		{
			*destructorCallCounter += 1;
		}
	}
};

TEST(ECSTestSuite, CreateEntity)
{
	ECS ecs;
	
	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
}

TEST(ECSTestSuite, CreateNonemptyEntity)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, 0.0f);
}

TEST(ECSTestSuite, CreateNonemptyEntityMultiple)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity<CompB, CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, 0.0f);
	EXPECT_FLOAT_EQ(ecs.getComponent<CompB>(entity)->a, 0.0f);
	EXPECT_EQ(ecs.getComponent<CompB>(entity)->b, 0);
}

TEST(ECSTestSuite, CreateNonemptyEntityCopy)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	CompA compA{};
	compA.a = 4.0f;

	auto entity = ecs.createEntity<CompA>(compA);
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, compA.a);
}

TEST(ECSTestSuite, CreateNonemptyEntityMultipleCopy)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	CompA compA{};
	compA.a = 4.0f;

	CompB compB{};
	compB.a = 8.0f;
	compB.b = 5;

	auto entity = ecs.createEntity<CompB, CompA>(compB, compA);
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, compA.a);
	EXPECT_FLOAT_EQ(ecs.getComponent<CompB>(entity)->a, compB.a);
	EXPECT_EQ(ecs.getComponent<CompB>(entity)->b, compB.b);
}

TEST(ECSTestSuite, CreateNonemptyEntityMove)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity<CompA>({ 4.0f });
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, 4.0f);
}

TEST(ECSTestSuite, CreateNonemptyEntityMultipleMove)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity<CompB, CompA>({ 8.0f, 5 }, { 4.0f });
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, 4.0f);
	EXPECT_FLOAT_EQ(ecs.getComponent<CompB>(entity)->a, 8.0f);
	EXPECT_EQ(ecs.getComponent<CompB>(entity)->b, 5);
}

TEST(ECSTestSuite, AddComponent) 
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponents<CompA>(entity));
}

TEST(ECSTestSuite, AddComponentAgain)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
}

TEST(ECSTestSuite, Add2Components)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	bool hasComps = false;

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
	hasComps = ecs.hasComponents<CompA, CompB>(entity);
	EXPECT_FALSE(hasComps);
	hasComps = ecs.hasComponents<CompB, CompA>(entity);
	EXPECT_FALSE(hasComps);

	ecs.addComponent<CompB>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	hasComps = ecs.hasComponents<CompA, CompB>(entity);
	EXPECT_TRUE(hasComps);
	hasComps = ecs.hasComponents<CompB, CompA>(entity);
	EXPECT_TRUE(hasComps);
}

TEST(ECSTestSuite, Add2ComponentsAgain)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));

	ecs.addComponent<CompB>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));

	ecs.addComponent<CompA>(entity);
	ecs.addComponent<CompB>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
}

TEST(ECSTestSuite, Add2ComponentsOverlap)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();
	ecs.registerComponent<CompC>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);

	ecs.addComponent<CompA>(entity);
	ecs.addComponent<CompB>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompC>(entity));

	ecs.addComponent<CompB>(entity);
	ecs.addComponent<CompC>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompC>(entity));
}

TEST(ECSTestSuite, AddComponentToNonempty)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));

	ecs.addComponent<CompB>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
}

TEST(ECSTestSuite, AddComponentToNonemptyAgain)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
}

TEST(ECSTestSuite, AddComponentToNonemptyOverlap)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity<CompA, CompB>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));

	ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
}

TEST(ECSTestSuite, AddComponentCopy)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	CompA compA{};
	compA.a = 4.0f;

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));

	ecs.addComponent<CompA>(entity, compA);
	ASSERT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, compA.a);
}

TEST(ECSTestSuite, AddComponentMultipleCopy)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	CompA compA{};
	compA.a = 4.0f;

	CompB compB{};
	compB.a = 8.0f;
	compB.b = 5;

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));

	ecs.addComponents<CompA, CompB>(entity, compA, compB);
	ASSERT_TRUE(ecs.hasComponent<CompA>(entity));
	ASSERT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, compA.a);
	EXPECT_FLOAT_EQ(ecs.getComponent<CompB>(entity)->a, compB.a);
	EXPECT_EQ(ecs.getComponent<CompB>(entity)->b, compB.b);
}

TEST(ECSTestSuite, AddComponentMove)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));

	CompA compA{ 4.0f };

	ecs.addComponent<CompA>(entity, eastl::move(compA));
	ASSERT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, 4.0f);
}

TEST(ECSTestSuite, AddComponentMultipleMove)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();
	
	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));

	ecs.addComponents<CompB, CompA>(entity, { 8.0f, 5 }, { 4.0f });
	ASSERT_TRUE(ecs.hasComponent<CompA>(entity));
	ASSERT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_FLOAT_EQ(ecs.getComponent<CompA>(entity)->a, 4.0f);
	EXPECT_FLOAT_EQ(ecs.getComponent<CompB>(entity)->a, 8.0f);
	EXPECT_EQ(ecs.getComponent<CompB>(entity)->b, 5);
}

TEST(ECSTestSuite, RemoveComponentPresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));

	EXPECT_TRUE(ecs.removeComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
}

TEST(ECSTestSuite, RemoveComponentNotPresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));

	EXPECT_FALSE(ecs.removeComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
}

TEST(ECSTestSuite, RemoveComponentMultipleAllPresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();
	ecs.registerComponent<CompC>();

	auto entity = ecs.createEntity<CompA, CompB, CompC>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompC>(entity));

	bool removeResult = ecs.removeComponents<CompA, CompB, CompC>(entity);
	EXPECT_TRUE(removeResult);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompC>(entity));
}

TEST(ECSTestSuite, RemoveComponentMultipleSomePresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();
	ecs.registerComponent<CompC>();

	auto entity = ecs.createEntity<CompA, CompC>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompC>(entity));

	bool removeResult = ecs.removeComponents<CompA, CompB, CompC>(entity);
	EXPECT_TRUE(removeResult);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompC>(entity));
}

TEST(ECSTestSuite, RemoveComponentMultipleNonePresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();
	ecs.registerComponent<CompC>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompC>(entity));

	bool removeResult = ecs.removeComponents<CompA, CompB, CompC>(entity);
	EXPECT_FALSE(removeResult);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompC>(entity));
}

TEST(ECSTestSuite, AddRemoveComponent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));

	ecs.addRemoveComponent<CompB, CompA>(entity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
}

TEST(ECSTestSuite, AddRemoveComponentNoRemove)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));

	ecs.addRemoveComponent<CompB, CompA>(entity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_TRUE(ecs.hasComponent<CompB>(entity));
}

TEST(ECSTestSuite, AddRemoveComponentNoAdd)
{
	ECS ecs;
	ecs.registerComponent<CompA>();
	ecs.registerComponent<CompB>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));

	ecs.addRemoveComponent<CompA, CompB>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_FALSE(ecs.hasComponent<CompB>(entity));
}

TEST(ECSTestSuite, AddRemoveSame)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity<CompA>();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));

	ecs.addRemoveComponent<CompA, CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
}

TEST(ECSTestSuite, GetComponentPresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));

	CompA *c = ecs.addComponent<CompA>(entity);
	EXPECT_TRUE(ecs.hasComponent<CompA>(entity));
	EXPECT_EQ(c, ecs.getComponent<CompA>(entity));
}

TEST(ECSTestSuite, GetComponentNotPresent)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
	EXPECT_FALSE(ecs.hasComponent<CompA>(entity));
	EXPECT_EQ(nullptr, ecs.getComponent<CompA>(entity));
}

TEST(ECSTestSuite, DestructorCallSingleComponent)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();

	int counter = 0;
	DestructorTestComp<0> c(&counter);

	auto entity = ecs.createEntity<DestructorTestComp<0>>(c);
	EXPECT_EQ(counter, 0);
	ecs.removeComponent<DestructorTestComp<0>>(entity);
	EXPECT_EQ(counter, 1);
}

TEST(ECSTestSuite, DestructorCallMultipleComponents)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();
	ecs.registerComponent<DestructorTestComp<1>>();

	int counter0 = 0;
	int counter1 = 0;
	DestructorTestComp<0> c0(&counter0);
	DestructorTestComp<1> c1(&counter1);

	auto entity = ecs.createEntity<DestructorTestComp<0>, DestructorTestComp<1>>(c0, c1);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.removeComponents<DestructorTestComp<0>, DestructorTestComp<1>>(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 1);
}

TEST(ECSTestSuite, DestructorCallMultipleComponentsMigrateAdd)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();
	ecs.registerComponent<DestructorTestComp<1>>();

	int counter0 = 0;
	int counter1 = 0;
	DestructorTestComp<0> c0(&counter0);
	DestructorTestComp<1> c1(&counter1);

	auto entity = ecs.createEntity<DestructorTestComp<0>>(c0);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.addComponent<DestructorTestComp<1>>(entity, c1);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
}

TEST(ECSTestSuite, DestructorCallMultipleComponentsMigrateRemove)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();
	ecs.registerComponent<DestructorTestComp<1>>();

	int counter0 = 0;
	int counter1 = 0;
	DestructorTestComp<0> c0(&counter0);
	DestructorTestComp<1> c1(&counter1);

	auto entity = ecs.createEntity<DestructorTestComp<0>, DestructorTestComp<1>>(c0, c1);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.removeComponent<DestructorTestComp<0>>(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 0);
	ecs.removeComponent<DestructorTestComp<1>>(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 1);
}

TEST(ECSTestSuite, DestructorCallMultipleComponentsMigrateAddAndRemove)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();
	ecs.registerComponent<DestructorTestComp<1>>();

	int counter0 = 0;
	int counter1 = 0;
	DestructorTestComp<0> c0(&counter0);
	DestructorTestComp<1> c1(&counter1);

	auto entity = ecs.createEntity<DestructorTestComp<0>>(c0);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.addComponent<DestructorTestComp<1>>(entity, c1);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.removeComponent<DestructorTestComp<0>>(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 0);
	ecs.removeComponent<DestructorTestComp<1>>(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 1);
}

TEST(ECSTestSuite, DestructorCallMultipleComponentsMigrateAddRemove)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();
	ecs.registerComponent<DestructorTestComp<1>>();

	int counter0 = 0;
	int counter1 = 0;
	DestructorTestComp<0> c0(&counter0);
	DestructorTestComp<1> c1(&counter1);

	auto entity = ecs.createEntity<DestructorTestComp<0>>(c0);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.addRemoveComponent<DestructorTestComp<1>, DestructorTestComp<0>>(entity, c1);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 0);
	ecs.removeComponent<DestructorTestComp<1>>(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 1);
}

TEST(ECSTestSuite, DestructorCallDestroyEntitySingleComponent)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();

	int counter = 0;
	DestructorTestComp<0> c(&counter);

	auto entity = ecs.createEntity<DestructorTestComp<0>>(c);
	EXPECT_EQ(counter, 0);
	ecs.destroyEntity(entity);
	EXPECT_EQ(counter, 1);
}

TEST(ECSTestSuite, DestructorCallDestroyEntityMultipleComponents)
{
	ECS ecs;
	ecs.registerComponent<DestructorTestComp<0>>();
	ecs.registerComponent<DestructorTestComp<1>>();

	int counter0 = 0;
	int counter1 = 0;
	DestructorTestComp<0> c0(&counter0);
	DestructorTestComp<1> c1(&counter1);

	auto entity = ecs.createEntity<DestructorTestComp<0>, DestructorTestComp<1>>(c0, c1);
	EXPECT_EQ(counter0, 0);
	EXPECT_EQ(counter1, 0);
	ecs.destroyEntity(entity);
	EXPECT_EQ(counter0, 1);
	EXPECT_EQ(counter1, 1);
}

TEST(ECSTestSuite, EmptyEntityCreate)
{
	ECS ecs;

	auto entity = ecs.createEntity();
	EXPECT_NE(entity, k_nullEntity);
}

TEST(ECSTestSuite, EmptyEntityCreateTemplated)
{
	ECS ecs;

	auto entity = ecs.createEntity<>();
	EXPECT_NE(entity, k_nullEntity);
}

TEST(ECSTestSuite, EmptyEntityIterate)
{
	ECS ecs;

	const auto entity = ecs.createEntity();

	size_t iterationCount = 0;
	ecs.iterate<>([&](size_t count, const EntityID *entities)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				EXPECT_EQ(entities[0], entity);
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 1);
}

TEST(ECSTestSuite, EmptyEntityAddComponentIterate)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	const auto entity = ecs.createEntity();
	ecs.addComponent<CompA>(entity);

	size_t iterationCount = 0;
	ecs.iterate<CompA>([&](size_t count, const EntityID *entities, CompA *c)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				EXPECT_EQ(entities[0], entity);
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 1);
}

TEST(ECSTestSuite, EmptyEntityAddComponentIterateRemoveComponentIterate)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	const auto entity = ecs.createEntity();
	ecs.addComponent<CompA>(entity);

	size_t iterationCount = 0;
	ecs.iterate<CompA>([&](size_t count, const EntityID *entities, CompA *c)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				EXPECT_EQ(entities[0], entity);
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 1);

	ecs.removeComponent<CompA>(entity);

	iterationCount = 0;
	ecs.iterate<>([&](size_t count, const EntityID *entities)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				EXPECT_EQ(entities[0], entity);
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 1);
}

TEST(ECSTestSuite, EmptyEntityIterateRemoveComponentIterate)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	const auto entity = ecs.createEntity<CompA>();

	size_t iterationCount = 0;
	ecs.iterate<CompA>([&](size_t count, const EntityID *entities, CompA *c)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				EXPECT_EQ(entities[0], entity);
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 1);

	ecs.removeComponent<CompA>(entity);

	iterationCount = 0;
	ecs.iterate<>([&](size_t count, const EntityID *entities)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				EXPECT_EQ(entities[0], entity);
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 1);
}

TEST(ECSTestSuite, EmptyEntityIterateOptional)
{
	ECS ecs;
	ecs.registerComponent<CompA>();

	const auto entity0 = ecs.createEntity<CompA>();
	const auto entity1 = ecs.createEntity<>();

	size_t iterationCount = 0;
	IterateQuery query;
	ecs.setIterateQueryOptionalComponents<CompA>(query);
	ecs.iterate<CompA>(query, [&](size_t count, const EntityID *entities, CompA *c)
		{
			EXPECT_EQ(count, 1);
			if (count >= 1)
			{
				if (c)
				{
					EXPECT_EQ(entities[0], entity0);
				}
				else
				{
					EXPECT_EQ(entities[0], entity1);
				}
			}
			++iterationCount;
		});

	EXPECT_EQ(iterationCount, 2);
}