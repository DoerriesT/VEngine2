#pragma once
#include <stdint.h>

class Window;
class Renderer;
class Physics;
class AnimationSystem;
class UserInput;
class IGameLogic;
class ECS;
class Level;

class Engine
{
public:
	int start(int argc, char *argv[], IGameLogic *gameLogic) noexcept;
	ECS *getECS() noexcept;
	Renderer *getRenderer() noexcept;
	Physics *getPhysics() noexcept;
	//UserInput *getUserInput() noexcept;
	Level *getLevel() noexcept;
	void setEditorMode(bool editorMode) noexcept;
	bool isEditorMode() const noexcept;
	void setEditorViewport(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height) noexcept;
	void setPickingPos(uint32_t x, uint32_t y) noexcept;
	void getResolution(uint32_t *swapchainWidth, uint32_t *swapchainHeight, uint32_t *width, uint32_t *height) noexcept;
	uint64_t getPickedEntity() const noexcept;
	void setCameraEntity(uint64_t cameraEntity) noexcept;
	uint64_t getCameraEntity() const noexcept;

private:
	IGameLogic *m_gameLogic = nullptr;
	Window *m_window = nullptr;
	ECS *m_ecs = nullptr;
	Renderer *m_renderer = nullptr;
	Physics *m_physics = nullptr;
	AnimationSystem *m_animationSystem = nullptr;
	UserInput *m_userInput = nullptr;
	Level *m_level = nullptr;
	bool m_editorMode = false;
	bool m_viewportParamsDirty = true;
	int32_t m_editorViewportOffsetX = 0;
	int32_t m_editorViewportOffsetY = 0;
	uint32_t m_editorViewportWidth = 0;
	uint32_t m_editorViewportHeight = 0;
	uint64_t m_pickedEntity = 0;
	uint64_t m_cameraEntity = 0;
};