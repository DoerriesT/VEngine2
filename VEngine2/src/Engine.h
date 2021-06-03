#pragma once
#include <stdint.h>

class Window;
class Renderer;
class UserInput;
class IGameLogic;
class ECS;

class Engine
{
public:
	int start(int argc, char *argv[], IGameLogic *gameLogic) noexcept;
	ECS *getECS() noexcept;
	Renderer *getRenderer() noexcept;
	UserInput *getUserInput() noexcept;
	void setEditorMode(bool editorMode) noexcept;
	bool isEditorMode() const noexcept;
	void setEditorViewport(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height) noexcept;
	void getResolution(uint32_t *swapchainWidth, uint32_t *swapchainHeight, uint32_t *width, uint32_t *height) noexcept;

private:
	IGameLogic *m_gameLogic = nullptr;
	Window *m_window = nullptr;
	ECS *m_ecs = nullptr;
	Renderer *m_renderer = nullptr;
	UserInput *m_userInput = nullptr;
	bool m_editorMode = false;
	bool m_viewportParamsDirty = true;
	int32_t m_editorViewportOffsetX = 0;
	int32_t m_editorViewportOffsetY = 0;
	uint32_t m_editorViewportWidth = 0;
	uint32_t m_editorViewportHeight = 0;
};