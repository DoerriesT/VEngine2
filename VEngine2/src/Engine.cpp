#include "Engine.h"
#include "Log.h"
#include "window/Window.h"
#include "graphics/Renderer.h"

int Engine::start(int argc, char *argv[])
{
	m_window = new Window(1600, 900, Window::WindowMode::WINDOWED, "VEngine 2");
	m_renderer = new Renderer(m_window->getWindowHandle(), m_window->getWidth(), m_window->getHeight());

	while (!m_window->shouldClose())
	{
		m_window->pollEvents();

		if (m_window->configurationChanged())
		{
			m_renderer->resize(m_window->getWidth(), m_window->getHeight());
		}

		m_renderer->render();
	}

	delete m_window;
	m_window = nullptr;
	delete m_renderer;
	m_renderer = nullptr;

	return 0;
}
