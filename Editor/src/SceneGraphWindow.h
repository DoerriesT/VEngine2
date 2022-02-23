#pragma once
#include <ecs/ECS.h>

class Engine;
class FPSCameraController;

class SceneGraphWindow
{
public:
	explicit SceneGraphWindow(Engine *engine) noexcept;
	void draw(bool viewportHasFocus) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;
	EntityID getSelectedEntity() const;

private:
	Engine *m_engine = nullptr;
	EntityID m_selectedEntity = k_nullEntity;
	EntityID m_toDeleteEntity = k_nullEntity;
	EntityID m_reparentChild = k_nullEntity;
	EntityID m_reparentParent = k_nullEntity;
	bool m_reparentDrop = false;
	bool m_visible = true;

	void drawTreeNode(EntityID entity, EntityID *selected) noexcept;
};