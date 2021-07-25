#include "FPSCameraController.h"
#include "graphics/Camera.h"
#include "UserInput.h"
#include <glm/gtc/type_ptr.hpp>

FPSCameraController::FPSCameraController(UserInput *userInput)
	:m_userInput(userInput)
{
}

void FPSCameraController::update(float timeDelta, Camera &camera)
{
	bool pressed = false;
	float mod = 1.0f;
	glm::vec3 cameraTranslation = glm::vec3(0.0f);

	glm::vec2 mouseDelta = {};

	if (m_userInput->isMouseButtonPressed(InputMouse::BUTTON_RIGHT))
	{
		if (!m_grabbedMouse)
		{
			m_grabbedMouse = true;
			//m_window->setMouseCursorMode(Window::MouseCursorMode::DISABLED);
		}
		mouseDelta = m_userInput->getMousePosDelta();
	}
	else
	{
		if (m_grabbedMouse)
		{
			m_grabbedMouse = false;
			//m_window->setMouseCursorMode(Window::MouseCursorMode::NORMAL);
		}
	}

	glm::vec2 mouseHistory = glm::mix(glm::make_vec2(m_mouseHistory), mouseDelta, timeDelta / (timeDelta + 0.05f));
	if (glm::dot(mouseHistory, mouseHistory) > 0.0f)
	{
		camera.rotate(glm::vec3(mouseHistory.y * 0.005f, mouseHistory.x * 0.005f, 0.0));
	}
	m_mouseHistory[0] = mouseHistory[0];
	m_mouseHistory[1] = mouseHistory[1];


	if (m_userInput->isKeyPressed(InputKey::UP))
	{
		camera.rotate(glm::vec3(-timeDelta, 0.0f, 0.0));
	}
	if (m_userInput->isKeyPressed(InputKey::DOWN))
	{
		camera.rotate(glm::vec3(timeDelta, 0.0f, 0.0));
	}
	if (m_userInput->isKeyPressed(InputKey::LEFT))
	{
		camera.rotate(glm::vec3(0.0f, -timeDelta, 0.0));
	}
	if (m_userInput->isKeyPressed(InputKey::RIGHT))
	{
		camera.rotate(glm::vec3(0.0f, timeDelta, 0.0));
	}

	if (m_userInput->isKeyPressed(InputKey::LEFT_SHIFT))
	{
		mod = 35.0f;
	}
	if (m_userInput->isKeyPressed(InputKey::W))
	{
		cameraTranslation.z = -mod * (float)timeDelta;
		pressed = true;
	}
	if (m_userInput->isKeyPressed(InputKey::S))
	{
		cameraTranslation.z = mod * (float)timeDelta;
		pressed = true;
	}
	if (m_userInput->isKeyPressed(InputKey::A))
	{
		cameraTranslation.x = -mod * (float)timeDelta;
		pressed = true;
	}
	if (m_userInput->isKeyPressed(InputKey::D))
	{
		cameraTranslation.x = mod * (float)timeDelta;
		pressed = true;
	}
	if (pressed)
	{
		camera.translate(cameraTranslation);
	}
}
