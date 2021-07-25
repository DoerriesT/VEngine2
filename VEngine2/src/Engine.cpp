#include "Engine.h"
#include "Log.h"
#include "window/Window.h"
#include "graphics/Renderer.h"
#include "input/UserInput.h"
#include <optick.h>
#include "utility/Timer.h"
#include "graphics/imgui/imgui.h"
#include "input/ImGuiInputAdapter.h"
#include "asset/AssetManager.h"
#include "asset/TextureAssetHandler.h"
#include "asset/TextureAsset.h"
#include "asset/MeshAssetHandler.h"
#include "asset/MeshAsset.h"
#include "utility/Utility.h"
#include "IGameLogic.h"
#include "ecs/ECS.h"
#include "ecs/ECSTypeIDTranslator.h"
#include "reflection/Reflection.h"
#include "component/TransformComponent.h"
#include "component/CameraComponent.h"
#include "component/LightComponent.h"
#include "component/MeshComponent.h"
#include "component/PhysicsComponent.h"
#include "physics/Physics.h"
#include "Level.h"
#include "file/FileDialog.h"

// these are needed for EASTL

void *__cdecl operator new[](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

void *__cdecl operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char *pName, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

template<typename T>
static void registerComponent(ECS *ecs)
{
	T::reflect(g_Reflection);
	ecs->registerComponent<T>();
	ECSTypeIDTranslator::registerType<T>();
}

int Engine::start(int argc, char *argv[], IGameLogic *gameLogic) noexcept
{
	FileDialog::initializeCOM();

	m_gameLogic = gameLogic;
	m_window = new Window(1600, 900, Window::WindowMode::WINDOWED, "VEngine 2");
	Timer::init();

	m_level = new Level();

	// Setup Dear ImGui context
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		io.ConfigDockingWithShift = true;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		//ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)m_window->getWindowHandle(), true);
	}

	m_ecs = new ECS();

	registerComponent<TransformComponent>(m_ecs);
	registerComponent<CameraComponent>(m_ecs);
	registerComponent<LightComponent>(m_ecs);
	registerComponent<MeshComponent>(m_ecs);
	registerComponent<PhysicsComponent>(m_ecs);

	m_renderer = new Renderer(m_ecs, m_window->getWindowHandle(), m_window->getWidth(), m_window->getHeight());
	m_physics = new Physics(m_ecs);
	m_userInput = new UserInput(*m_window);
	AssetManager::init();

	TextureAssetHandler::init(AssetManager::get(), m_renderer);
	MeshAssetHandler::init(AssetManager::get(), m_renderer, m_physics);

	ImGuiInputAdapter imguiInputAdapter(ImGui::GetCurrentContext(), *m_userInput, *m_window);
	imguiInputAdapter.resize(m_window->getWidth(), m_window->getHeight(), m_window->getWindowWidth(), m_window->getWindowHeight());

	m_gameLogic->init(this);

	Timer timer;

	while (!m_window->shouldClose())
	{
		OPTICK_FRAME("MainThread");

		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		m_window->pollEvents();

		if (m_window->configurationChanged() || m_viewportParamsDirty)
		{
			m_viewportParamsDirty = false;

			imguiInputAdapter.resize(m_window->getWidth(), m_window->getHeight(), m_window->getWindowWidth(), m_window->getWindowHeight());
			m_renderer->resize(m_window->getWidth(), m_window->getHeight(), m_editorMode ? m_editorViewportWidth : m_window->getWidth(), m_editorMode ? m_editorViewportHeight : m_window->getHeight());
		}

		m_userInput->input();
		imguiInputAdapter.update();
		ImGui::NewFrame();


		ImGui::ShowDemoWindow();

		m_physics->update(timeDelta);

		m_gameLogic->update(timeDelta);

		ImGui::Render();

		m_renderer->render();
	}

	m_gameLogic->shutdown();
	TextureAssetHandler::shutdown();
	MeshAssetHandler::shutdown();
	AssetManager::shutdown();
	delete m_userInput;
	delete m_renderer;
	delete m_ecs;
	delete m_window;
	delete m_level;

	return 0;
}

ECS *Engine::getECS() noexcept
{
	return m_ecs;
}

Renderer *Engine::getRenderer() noexcept
{
	return m_renderer;
}

Physics *Engine::getPhysics() noexcept
{
	return m_physics;
}

UserInput *Engine::getUserInput() noexcept
{
	return m_userInput;
}

Level *Engine::getLevel() noexcept
{
	return m_level;
}

void Engine::setEditorMode(bool editorMode) noexcept
{
	m_viewportParamsDirty = true;
	m_editorMode = editorMode;
	m_renderer->setEditorMode(editorMode);
}

bool Engine::isEditorMode() const noexcept
{
	return m_editorMode;
}

void Engine::setEditorViewport(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height) noexcept
{
	if (m_editorViewportOffsetX != offsetX || m_editorViewportOffsetY != offsetY || m_editorViewportWidth != width || m_editorViewportHeight != height)
	{
		m_viewportParamsDirty = true;
		m_editorViewportOffsetX = offsetX;
		m_editorViewportOffsetY = offsetY;
		m_editorViewportWidth = width;
		m_editorViewportHeight = height;
	}
}

void Engine::getResolution(uint32_t *swapchainWidth, uint32_t *swapchainHeight, uint32_t *width, uint32_t *height) noexcept
{
	m_renderer->getResolution(swapchainWidth, swapchainHeight, width, height);
}
