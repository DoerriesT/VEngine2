#pragma once
#include "ecs/ECS.h"

class ECS;
class Camera;
struct CharacterControllerComponent;

class FPSCameraController
{
public:
	explicit FPSCameraController(ECS *ecs);
	void update(float timeDelta, Camera &camera, CharacterControllerComponent *ccc = nullptr);

private:
	ECS *m_ecs = nullptr;
	float m_mouseHistory[2] = {};
	bool m_grabbedMouse = false;
	
};