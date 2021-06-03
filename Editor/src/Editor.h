#pragma once
#include <IGameLogic.h>
#include <ecs/ECS.h>

class InspectorWindow;
class ViewportWindow;

class Editor : public IGameLogic
{
public:
	explicit Editor(IGameLogic *gameLogic) noexcept;
	void init(Engine *engine) noexcept override;
	void update(float deltaTime) noexcept override;
	void shutdown() noexcept override;

private:
	IGameLogic *m_gameLogic = nullptr;
	Engine *m_engine = nullptr;
	ViewportWindow *m_viewportWindow = nullptr;
	InspectorWindow *m_inspectorWindow = nullptr;
	EntityID m_editorCameraEntity = k_nullEntity;
};