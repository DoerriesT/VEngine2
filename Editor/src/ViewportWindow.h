#pragma once
#include <ecs/ECS.h>

class Engine;
class FlyCameraController;

class ViewportWindow
{
public:
	explicit ViewportWindow(Engine *engine, const EntityID *editorCameraEntity) noexcept;
	~ViewportWindow() noexcept;
	void draw(float deltaTime, bool gameIsPlaying, EntityID selectedEntity) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;
	bool hasFocus() const noexcept;

private:
	Engine *m_engine = nullptr;
	const EntityID *m_editorCameraEntityPtr = nullptr;
	FlyCameraController *m_flyCameraController = nullptr;
	bool m_visible = true;
	bool m_hasFocus = false;
	int m_gizmoType = 0; // translate = 0, rotate = 1, scale = 2
	bool m_gizmoLocal = false;
	float m_translateSnap = 0.1f;
	float m_rotateSnap = 10.0f; // degrees
	float m_scaleSnap = 0.25f;
};