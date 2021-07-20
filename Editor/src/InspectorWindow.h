#pragma once
#include <ecs/ECS.h>

class Engine;
struct SceneGraphNode;

class InspectorWindow
{
public:
	explicit InspectorWindow(Engine *engine) noexcept;
	void draw(EntityID  entity) noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	Engine *m_engine = nullptr;
	char m_nameStringTemp[256] = {};
	EntityID m_lastDisplayedEntity = k_nullEntity;
	SceneGraphNode *m_currentSceneGraphNode = nullptr;
	bool m_visible = true;
};