#pragma once

class Window;
class Renderer;

class Engine
{
public:
	int start(int argc, char *argv[]);

private:
	Window *m_window = nullptr;
	Renderer *m_renderer = nullptr;
};