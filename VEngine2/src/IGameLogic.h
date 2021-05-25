#pragma once

class Engine;

class IGameLogic
{
public:
	virtual void init(Engine *engine) noexcept = 0;
	virtual void update(float deltaTime) noexcept = 0;
	virtual void shutdown() noexcept = 0;
};