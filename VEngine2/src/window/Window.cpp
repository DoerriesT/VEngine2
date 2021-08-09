#include "Window.h"
#include <assert.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "utility/Utility.h"
#include "utility/ContainerUtility.h"
#include "input/IInputListener.h"

void windowSizeCallback(GLFWwindow *window, int width, int height);
void framebufferSizeCallback(GLFWwindow *window, int width, int height);
void curserPosCallback(GLFWwindow *window, double xPos, double yPos);
void curserEnterCallback(GLFWwindow *window, int entered);
void scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
void charCallback(GLFWwindow *window, unsigned int codepoint);
void joystickCallback(int joystickId, int event);

Window::Window(unsigned int width, unsigned int height, WindowMode windowMode, const std::string &title)
	:m_windowHandle(),
	m_cursors(),
	m_windowMode(windowMode),
	m_newWindowMode(windowMode),
	m_windowWidth(width),
	m_windowHeight(height),
	m_width(width),
	m_height(height),
	m_windowedWidth(width),
	m_windowedHeight(height),
	m_title(title),
	m_configurationChanged(),
	m_gamepads(16)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	switch (m_windowMode)
	{
	case WindowMode::WINDOWED:
	{
		m_windowHandle = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
		break;
	}
	case WindowMode::FULL_SCREEN:
	{
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);

		m_windowHandle = glfwCreateWindow(mode->width, mode->height, m_title.c_str(), glfwGetPrimaryMonitor(), nullptr);
		break;
	}
	default:
		assert(false);
		break;
	}

	if (!m_windowHandle)
	{
		glfwTerminate();
		util::fatalExit("Failed to create GLFW window", EXIT_FAILURE);
		return;
	}

	glfwSetWindowSizeCallback(m_windowHandle, windowSizeCallback);
	glfwSetFramebufferSizeCallback(m_windowHandle, framebufferSizeCallback);
	glfwSetCursorPosCallback(m_windowHandle, curserPosCallback);
	glfwSetCursorEnterCallback(m_windowHandle, curserEnterCallback);
	glfwSetScrollCallback(m_windowHandle, scrollCallback);
	glfwSetMouseButtonCallback(m_windowHandle, mouseButtonCallback);
	glfwSetKeyCallback(m_windowHandle, keyCallback);
	glfwSetCharCallback(m_windowHandle, charCallback);
	glfwSetJoystickCallback(joystickCallback);

	glfwSetWindowUserPointer(m_windowHandle, this);

	// get actual width/height
	{
		int w = 0;
		int h = 0;

		glfwGetWindowSize(m_windowHandle, &w, &h);
		m_windowWidth = static_cast<uint32_t>(w);
		m_windowHeight = static_cast<uint32_t>(h);

		glfwGetFramebufferSize(m_windowHandle, &w, &h);
		m_width = static_cast<uint32_t>(w);
		m_height = static_cast<uint32_t>(h);
	}

	// create cursors
	m_cursors[(size_t)MouseCursor::ARROW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
	m_cursors[(size_t)MouseCursor::TEXT] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
	m_cursors[(size_t)MouseCursor::RESIZE_ALL] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);   // FIXME: GLFW doesn't have this.
	m_cursors[(size_t)MouseCursor::RESIZE_VERTICAL] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
	m_cursors[(size_t)MouseCursor::RESIZE_HORIZONTAL] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
	m_cursors[(size_t)MouseCursor::RESIZE_TRBL] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
	m_cursors[(size_t)MouseCursor::RESIZE_TLBR] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);  // FIXME: GLFW doesn't have this.
	m_cursors[(size_t)MouseCursor::HAND] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
	m_cursors[(size_t)MouseCursor::CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
}

Window::~Window()
{
	for (size_t i = 0; i < (sizeof(m_cursors) / sizeof(m_cursors[0])); ++i)
	{
		glfwDestroyCursor(m_cursors[i]);
	}

	glfwDestroyWindow(m_windowHandle);
	glfwTerminate();
}

void Window::pollEvents()
{
	m_configurationChanged = false;
	glfwPollEvents();

	if (m_newWindowMode != m_windowMode)
	{
		switch (m_newWindowMode)
		{
		case Window::WindowMode::WINDOWED:
		{
			const GLFWvidmode *vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
			int x = vidMode->width / 2 - m_windowedWidth / 2;
			int y = vidMode->height / 2 - m_windowedHeight / 2;
			if (y < 32)
			{
				y = 32;
			}
			glfwSetWindowMonitor(m_windowHandle, nullptr, x, y, m_windowedWidth, m_windowedHeight, GLFW_DONT_CARE);
			break;
		}
		case Window::WindowMode::FULL_SCREEN:
		{
			m_windowedWidth = m_windowWidth;
			m_windowedHeight = m_windowHeight;
			const GLFWvidmode *vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
			glfwSetWindowMonitor(m_windowHandle, glfwGetPrimaryMonitor(), 0, 0, vidMode->width, vidMode->height, GLFW_DONT_CARE);
			break;
		}
		default:
			assert(false);
			break;
		}

		m_configurationChanged = true;
		m_windowMode = m_newWindowMode;
	}

	// gamepads
	{
		const float *axisValues;
		const unsigned char *buttonValues;
		int axisCount;
		int buttonCount;

		for (int i = 0; i < m_gamepads.size(); ++i)
		{
			bool connected = glfwJoystickPresent(i);

			axisValues = glfwGetJoystickAxes(i, &axisCount);
			buttonValues = glfwGetJoystickButtons(i, &buttonCount);

			if (connected && axisCount == 6 && buttonCount == 14)
			{
				/*
				buttons:
				0 = A
				1 = B
				2 = X
				3 = Y
				4 = LB
				5 = RB
				6 = back
				7 = start
				8 = left stick
				9 = right stick
				10 = up
				11 = right
				12 = down
				13 = left

				axes:
				0 = left x
				1 = left y
				2 = right x
				3 = right y
				4 = LT
				5 = RT
				*/

				m_gamepads[i].m_id = i;
				m_gamepads[i].m_buttonA = buttonValues[0];
				m_gamepads[i].m_buttonB = buttonValues[1];
				m_gamepads[i].m_buttonX = buttonValues[2];
				m_gamepads[i].m_buttonY = buttonValues[3];
				m_gamepads[i].m_leftButton = buttonValues[4];
				m_gamepads[i].m_rightButton = buttonValues[5];
				m_gamepads[i].m_backButton = buttonValues[6];
				m_gamepads[i].m_startButton = buttonValues[7];
				m_gamepads[i].m_leftStick = buttonValues[8];
				m_gamepads[i].m_rightStick = buttonValues[9];
				m_gamepads[i].m_dPadUp = buttonValues[10];
				m_gamepads[i].m_dPadRight = buttonValues[11];
				m_gamepads[i].m_dPadDown = buttonValues[12];
				m_gamepads[i].m_dPadLeft = buttonValues[13];
				m_gamepads[i].m_leftStickX = axisValues[0];
				m_gamepads[i].m_leftStickY = axisValues[1];
				m_gamepads[i].m_rightStickX = axisValues[2];
				m_gamepads[i].m_rightStickY = axisValues[3];
				m_gamepads[i].m_leftTrigger = axisValues[4];
				m_gamepads[i].m_rightTrigger = axisValues[5];
			}
			else
			{
				m_gamepads[i].m_id = -1;
			}
		}
	}
}

void *Window::getWindowHandle() const
{
	return static_cast<void *>(m_windowHandle);
}

void *Window::getWindowHandleRaw() const
{
#ifdef _WIN32
	return glfwGetWin32Window(m_windowHandle);
#else
	return nullptr;
#endif
}

unsigned int Window::getWidth() const
{
	return m_width;
}

unsigned int Window::getHeight() const
{
	return m_height;
}

unsigned int Window::getWindowWidth() const
{
	return m_windowWidth;
}

unsigned int Window::getWindowHeight() const
{
	return m_windowHeight;
}

bool Window::shouldClose() const
{
	return glfwWindowShouldClose(m_windowHandle);
}

bool Window::configurationChanged() const
{
	return m_configurationChanged;
}

void Window::setMouseCursorMode(MouseCursorMode mode)
{
	switch (mode)
	{
	case MouseCursorMode::NORMAL:
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		break;
	case  MouseCursorMode::HIDDEN:
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		break;
	case  MouseCursorMode::DISABLED:
		glfwSetInputMode(m_windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		break;
	default:
		assert(false);
		break;
	}

	m_mouseCursorMode = mode;
}

Window::MouseCursorMode Window::getMouseCursorMode() const
{
	return m_mouseCursorMode;
}

void Window::setWindowMode(WindowMode windowMode)
{
	m_newWindowMode = windowMode;
}

Window::WindowMode Window::getWindowMode() const
{
	return m_windowMode;
}

void Window::setTitle(const std::string &title)
{
	m_title = title;
	glfwSetWindowTitle(m_windowHandle, m_title.c_str());
}

void Window::addInputListener(IInputListener *listener)
{
	m_inputListeners.push_back(listener);
	listener->onGamepadStateUpdate(m_gamepads.size(), m_gamepads.data());
}

void Window::removeInputListener(IInputListener *listener)
{
	ContainerUtility::remove(m_inputListeners, listener);
}

const char *Window::getClipboardText() const
{
	return glfwGetClipboardString(m_windowHandle);
}

void Window::setClipboardText(const char *text)
{
	glfwSetClipboardString(m_windowHandle, text);
}

void Window::setMouseCursor(MouseCursor cursor)
{
	assert((size_t)cursor < (sizeof(m_cursors) / sizeof(m_cursors[0])));
	glfwSetCursor(m_windowHandle, m_cursors[(size_t)cursor]);
}

void Window::setCursorPos(float x, float y)
{
	glfwSetCursorPos(m_windowHandle, x, y);
}

InputAction Window::getMouseButton(InputMouse mouseButton)
{
	return (InputAction)glfwGetMouseButton(m_windowHandle, (int)mouseButton);
}

bool Window::isFocused()
{
	return glfwGetWindowAttrib(m_windowHandle, GLFW_FOCUSED) != 0;
}

// callback functions

void windowSizeCallback(GLFWwindow *window, int width, int height)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	windowFramework->m_windowWidth = width;
	windowFramework->m_windowHeight = height;
	windowFramework->m_configurationChanged = true;
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	windowFramework->m_width = width;
	windowFramework->m_height = height;
	windowFramework->m_configurationChanged = true;
}

void curserPosCallback(GLFWwindow *window, double xPos, double yPos)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseMove(xPos, yPos);
	}
}

void curserEnterCallback(GLFWwindow *window, int entered)
{
}

void scrollCallback(GLFWwindow *window, double xOffset, double yOffset)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseScroll(xOffset, yOffset);
	}
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onMouseButton(static_cast<InputMouse>(button), static_cast<InputAction>(action));
	}
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onKey(static_cast<InputKey>(key), static_cast<InputAction>(action));
	}
}

void charCallback(GLFWwindow *window, unsigned int codepoint)
{
	Window *windowFramework = static_cast<Window *>(glfwGetWindowUserPointer(window));
	for (IInputListener *listener : windowFramework->m_inputListeners)
	{
		listener->onChar(codepoint);
	}
}

void joystickCallback(int joystickId, int event)
{
	if (event == GLFW_CONNECTED)
	{

	}
	else if (event == GLFW_DISCONNECTED)
	{

	}
}
