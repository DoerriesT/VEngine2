#pragma once
#include <EASTL/vector.h>

class ECS;

class AnimationSystem
{
public:
	explicit AnimationSystem(ECS *ecs) noexcept;
	~AnimationSystem() noexcept;
	void update(float deltaTime) noexcept;

private:
	ECS *m_ecs = nullptr;
};