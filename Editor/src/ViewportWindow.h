#pragma once
#include <ecs/ECS.h>

class Engine;
class FPSCameraController;

class ViewportWindow
{
public:
	explicit ViewportWindow(Engine *engine, const EntityID *editorCameraEntity) noexcept;
	~ViewportWindow() noexcept;
	void draw(float deltaTime) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	Engine *m_engine = nullptr;
	const EntityID *m_editorCameraEntityPtr = nullptr;
	FPSCameraController *m_fpsCameraController = nullptr;
	bool m_visible = true;
};