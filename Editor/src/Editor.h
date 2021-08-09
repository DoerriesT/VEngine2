#pragma once
#include <IGameLogic.h>
#include <ecs/ECS.h>

class InspectorWindow;
class ViewportWindow;
class AssetBrowserWindow;
class SceneGraphWindow;

class Editor : public IGameLogic
{
public:
	explicit Editor(IGameLogic *gameLogic) noexcept;
	void init(Engine *engine) noexcept override;
	void setPlaying(bool playing) noexcept override;
	void update(float deltaTime) noexcept override;
	void shutdown() noexcept override;

private:
	IGameLogic *m_gameLogic = nullptr;
	Engine *m_engine = nullptr;
	ViewportWindow *m_viewportWindow = nullptr;
	InspectorWindow *m_inspectorWindow = nullptr;
	AssetBrowserWindow *m_assetBrowserWindow = nullptr;
	SceneGraphWindow *m_sceneGraphWindow = nullptr;
	EntityID m_editorCameraEntity = k_nullEntity;
	bool m_gameIsPlaying = false;
};