#pragma once

class Window;
class Renderer;
class UserInput;
class IGameLogic;

class Engine
{
public:
	int start(int argc, char *argv[], IGameLogic *gameLogic);

private:
	IGameLogic *m_gameLogic = nullptr;
	Window *m_window = nullptr;
	Renderer *m_renderer = nullptr;
	UserInput *m_userInput = nullptr;
};