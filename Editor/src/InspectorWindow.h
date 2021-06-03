#pragma once
#include <ecs/ECS.h>

class Engine;

class InspectorWindow
{
public:
	explicit InspectorWindow(Engine *engine) noexcept;
	void draw(EntityID  entity) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	Engine *m_engine = nullptr;
	EntityID m_lastDisplayedEntity = k_nullEntity;
	bool m_visible = true;
};