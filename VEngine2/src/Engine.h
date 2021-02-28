#pragma once

class Window;
class Renderer;
class UserInput;

class Engine
{
public:
	int start(int argc, char *argv[]);

private:
	Window *m_window = nullptr;
	Renderer *m_renderer = nullptr;
	UserInput *m_userInput = nullptr;
};