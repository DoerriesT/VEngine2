#pragma once
#include <vector>
#include "input/IInputListener.h"
#include "input/GamepadState.h"
#include <string>

struct GLFWwindow;
struct GLFWcursor;

class Window
{
	friend void windowSizeCallback(GLFWwindow *window, int width, int height);
	friend void framebufferSizeCallback(GLFWwindow *window, int width, int height);
	friend void curserPosCallback(GLFWwindow *window, double xPos, double yPos);
	friend void curserEnterCallback(GLFWwindow *window, int entered);
	friend void scrollCallback(GLFWwindow *window, double xOffset, double yOffset);
	friend void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
	friend void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
	friend void charCallback(GLFWwindow *window, unsigned int codepoint);
	friend void joystickCallback(int joystickId, int event);
public:
	enum class MouseCursor
	{
		ARROW,
		TEXT,
		RESIZE_ALL,
		RESIZE_VERTICAL,
		RESIZE_HORIZONTAL,
		RESIZE_TRBL,
		RESIZE_TLBR,
		HAND,
		CROSSHAIR
	};

	enum class MouseCursorMode
	{
		NORMAL, HIDDEN, DISABLED
	};

	enum class WindowMode
	{
		WINDOWED, FULL_SCREEN
	};

	explicit Window(unsigned int width, unsigned int height, WindowMode windowMode, const std::string &title);
	~Window();
	void pollEvents();
	void *getWindowHandle() const;
	void *getWindowHandleRaw() const;
	unsigned int getWidth() const;
	unsigned int getHeight() const;
	unsigned int getWindowWidth() const;
	unsigned int getWindowHeight() const;
	bool shouldClose() const;
	bool configurationChanged() const;
	void setMouseCursorMode(MouseCursorMode mode);
	MouseCursorMode getMouseCursorMode() const;
	void setWindowMode(WindowMode windowMode);
	WindowMode getWindowMode() const;
	void setTitle(const std::string &title);
	void addInputListener(IInputListener *listener);
	void removeInputListener(IInputListener *listener);
	const char *getClipboardText() const;
	void setClipboardText(const char *text);
	void setMouseCursor(MouseCursor cursor);
	void setCursorPos(float x, float y);
	InputAction getMouseButton(InputMouse mouseButton);
	bool isFocused();

private:
	GLFWwindow *m_windowHandle;
	GLFWcursor *m_cursors[9];
	WindowMode m_windowMode;
	WindowMode m_newWindowMode;
	unsigned int m_windowWidth;
	unsigned int m_windowHeight;
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_windowedWidth;
	unsigned int m_windowedHeight;
	std::string m_title;
	std::vector<IInputListener *> m_inputListeners;
	std::vector<GamepadState> m_gamepads;
	bool m_configurationChanged;
	MouseCursorMode m_mouseCursorMode = MouseCursorMode::NORMAL;
};