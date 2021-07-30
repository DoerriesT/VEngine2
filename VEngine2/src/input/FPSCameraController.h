#pragma once
#include "ecs/ECS.h"

class UserInput;
class Camera;
struct CharacterControllerComponent;

class FPSCameraController
{
public:
	explicit FPSCameraController(UserInput *userInput);
	void update(float timeDelta, Camera &camera, CharacterControllerComponent *ccc = nullptr);

private:
	UserInput *m_userInput = nullptr;
	float m_mouseHistory[2] = {};
	bool m_grabbedMouse = false;
	
};