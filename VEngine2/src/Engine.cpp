#include "Engine.h"
#include "Log.h"
#include "window/Window.h"
#include "graphics/Renderer.h"
#include "input/UserInput.h"
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <optick.h>

namespace
{
	class Camera
	{
	public:
		explicit Camera()
		{
			updateViewMatrix();
			updateProjectionMatrix();
		}
		void setRotation(const glm::quat &rotation)
		{
			m_orientation = rotation;
			updateViewMatrix();
		}
		void setPosition(const glm::vec3 &position)
		{
			m_position = position;
			updateViewMatrix();
		}
		void setAspectRatio(float aspectRatio)
		{
			m_aspectRatio = aspectRatio;
			updateProjectionMatrix();
		}
		void setFovy(float fovy)
		{
			m_fovy = fovy;
			updateProjectionMatrix();
		}
		void rotate(const glm::vec3 &pitchYawRollOffset)
		{
			glm::quat tmp = glm::quat(glm::vec3(-pitchYawRollOffset.x, 0.0, 0.0));
			glm::quat tmp1 = glm::quat(glm::angleAxis(-pitchYawRollOffset.y, glm::vec3(0.0, 1.0, 0.0)));
			m_orientation = glm::normalize(tmp1 * m_orientation * tmp);

			updateViewMatrix();
		}
		void translate(const glm::vec3 &translationOffset)
		{
			glm::vec3 forward(m_viewMatrix[0][2], m_viewMatrix[1][2], m_viewMatrix[2][2]);
			glm::vec3 strafe(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]);

			m_position += translationOffset.z * forward + translationOffset.x * strafe;
			updateViewMatrix();
		}
		void lookAt(const glm::vec3 &targetPosition)
		{
			const glm::vec3 up(0.0, 1.0f, 0.0);

			m_viewMatrix = glm::lookAt(m_position, targetPosition, up);
			m_orientation = glm::quat_cast(m_viewMatrix);
		}
		glm::mat4 getViewMatrix() const
		{
			return m_viewMatrix;
		}
		glm::mat4 getProjectionMatrix() const
		{
			return m_projectionMatrix;
		}
		glm::vec3 getPosition() const
		{
			return m_position;
		}
		glm::quat getRotation() const
		{
			return m_orientation;
		}
		glm::vec3 getForwardDirection() const
		{
			return -glm::vec3(m_viewMatrix[0][2], m_viewMatrix[1][2], m_viewMatrix[2][2]);
		}
		glm::vec3 getUpDirection() const
		{
			return glm::vec3(m_viewMatrix[0][1], m_viewMatrix[1][1], m_viewMatrix[2][1]);
		}
		float getAspectRatio() const
		{
			return m_aspectRatio;
		}
		float getFovy() const
		{
			return m_fovy;
		}

	private:
		glm::mat4 m_viewMatrix = glm::mat4(1.0f);
		glm::mat4 m_projectionMatrix = glm::mat4(1.0f);
		glm::vec3 m_position = {};
		glm::quat m_orientation = glm::quat(glm::vec3(0.0f));
		float m_aspectRatio = 1.0f;
		float m_fovy = glm::pi<float>() * 0.5f;
		float m_near = 0.1f;
		float m_far = 1000.0f;

		void updateViewMatrix()
		{
			m_viewMatrix = glm::mat4_cast(glm::inverse(m_orientation)) * glm::translate(-m_position);
		}
		void updateProjectionMatrix()
		{
			m_projectionMatrix = glm::perspective(m_fovy, m_aspectRatio, m_far, m_near);
		}
	};
}

int Engine::start(int argc, char *argv[])
{
	m_window = new Window(1600, 900, Window::WindowMode::WINDOWED, "VEngine 2");
	m_renderer = new Renderer(m_window->getWindowHandle(), m_window->getWidth(), m_window->getHeight());
	m_userInput = new UserInput(*m_window);

	bool grabbedMouse = false;
	glm::vec2 mouseHistory{};
	Camera camera;
	camera.setPosition(glm::vec3(0.0f, 2.0f, 0.0f));

	while (!m_window->shouldClose())
	{
		OPTICK_FRAME("MainThread");

		m_window->pollEvents();
		m_userInput->input();
		{
			bool pressed = false;
			float mod = 1.0f;
			glm::vec3 cameraTranslation = glm::vec3(0.0f);

			glm::vec2 mouseDelta = {};

			if (m_userInput->isMouseButtonPressed(InputMouse::BUTTON_RIGHT))
			{
				if (!grabbedMouse)
				{
					grabbedMouse = true;
					m_window->setMouseCursorMode(Window::MouseCursorMode::DISABLED);
				}
				mouseDelta = m_userInput->getMousePosDelta();
			}
			else
			{
				if (grabbedMouse)
				{
					grabbedMouse = false;
					m_window->setMouseCursorMode(Window::MouseCursorMode::NORMAL);
				}
			}

			const float timeDelta = 1.0f / 60.0f;
			mouseHistory = glm::mix(mouseHistory, mouseDelta, timeDelta / (timeDelta + 0.05f));
			if (glm::dot(mouseHistory, mouseHistory) > 0.0f)
			{
				camera.rotate(glm::vec3(mouseHistory.y * 0.005f, mouseHistory.x * 0.005f, 0.0));
			}


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
				mod = 5.0f;
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

		if (m_window->configurationChanged())
		{
			m_renderer->resize(m_window->getWidth(), m_window->getHeight());
		}

		camera.setAspectRatio(m_window->getWidth() / (float)m_window->getHeight());
		auto viewMatrix = camera.getViewMatrix();
		auto projMatrix = camera.getProjectionMatrix();
		auto pos = camera.getPosition();
		m_renderer->render(&viewMatrix[0][0], &projMatrix[0][0], &pos[0]);
	}

	delete m_window;
	m_window = nullptr;
	delete m_renderer;
	m_renderer = nullptr;

	return 0;
}
