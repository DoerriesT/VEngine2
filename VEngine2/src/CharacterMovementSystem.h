#pragma once

class ECS;

class CharacterMovementSystem
{
public:
	explicit CharacterMovementSystem(ECS *ecs) noexcept;
	void update(float timeDelta) noexcept;

private:
	ECS *m_ecs;
};