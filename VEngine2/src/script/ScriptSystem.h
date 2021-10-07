#pragma once

class ECS;

class ScriptSystem
{
public:
	explicit ScriptSystem(ECS *ecs) noexcept;
	~ScriptSystem() noexcept;
	void update(float deltaTime) noexcept;

private:
	ECS *m_ecs = nullptr;
};