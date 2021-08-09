#pragma once
#include "ecs/ECS.h"

class ECS;
class Camera;

class FlyCameraController
{
public:
	explicit FlyCameraController(ECS *ecs);
	void update(float timeDelta, Camera &camera);

private:
	ECS *m_ecs = nullptr;
	float m_rotateHistory[2] = {};
};