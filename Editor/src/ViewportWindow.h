#pragma once
#include <ecs/ECS.h>

class Engine;
class FlyCameraController;

class ViewportWindow
{
public:
	explicit ViewportWindow(Engine *engine, const EntityID *editorCameraEntity) noexcept;
	~ViewportWindow() noexcept;
	void draw(float deltaTime, bool gameIsPlaying) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	Engine *m_engine = nullptr;
	const EntityID *m_editorCameraEntityPtr = nullptr;
	FlyCameraController *m_flyCameraController = nullptr;
	bool m_visible = true;
};