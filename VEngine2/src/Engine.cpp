#define NOMINMAX
#include "Engine.h"
#include "Log.h"
#include "window/Window.h"
#include "graphics/Renderer.h"
#include "input/UserInput.h"
#include "utility/Timer.h"
#include "graphics/imgui/imgui.h"
#include "input/ImGuiInputAdapter.h"
#include "asset/AssetManager.h"
#include "utility/Utility.h"
#include "IGameLogic.h"
#include "ecs/ECS.h"
#include "component/ComponentRegistration.h"
#include "physics/Physics.h"
#include "animation/AnimationSystem.h"
#include "Level.h"
#include "asset/handler/AssetHandlerRegistration.h"
#include "file/FileDialog.h"
#include "CharacterMovementSystem.h"
#include "filesystem/RawFileSystem.h"
#include "filesystem/VirtualFileSystem.h"
#include "profiling/Profiling.h"

// these are needed for EASTL

void *__cdecl operator new[](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

void *__cdecl operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char *pName, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

void *operator new(std::size_t count)
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new[](std::size_t count)
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new(std::size_t count, std::align_val_t al)
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new[](std::size_t count, std::align_val_t al)
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new(std::size_t count, const std::nothrow_t &) noexcept
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new[](std::size_t count, const std::nothrow_t &) noexcept
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new(std::size_t count, std::align_val_t al, const std::nothrow_t &) noexcept
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void *operator new[](std::size_t count, std::align_val_t al, const std::nothrow_t &) noexcept
{
	auto ptr = malloc(count);
	PROFILING_MEM_ALLOC(ptr, count);
	return ptr;
}

void operator delete  (void *ptr) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete[](void *ptr) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete  (void *ptr, std::align_val_t al) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete[](void *ptr, std::align_val_t al) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete  (void *ptr, std::size_t sz) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete[](void *ptr, std::size_t sz) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete  (void *ptr, std::size_t sz, std::align_val_t al) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

void operator delete[](void *ptr, std::size_t sz, std::align_val_t al) noexcept
{
	PROFILING_MEM_FREE(ptr);
	free(ptr);
}

namespace EA
{
	namespace StdC
	{
		int __cdecl Vsnprintf(char *__restrict buffer, size_t bufsz, char const *__restrict format, va_list args)
		{
			return _vsnprintf_s(buffer, bufsz, bufsz, format, args);
		}
	}
}

int Engine::start(int argc, char *argv[], IGameLogic *gameLogic) noexcept
{
	FileDialog::initializeCOM();

	// set VFS mount points
	{
		char currentPath[IFileSystem::k_maxPathLength] = {};
		RawFileSystem::get().getCurrentPath(currentPath);
		VirtualFileSystem::get().mount((std::string(currentPath) + "/assets").c_str(), "assets");
	}


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

	ComponentRegistration::registerAllComponents(m_ecs);

	m_renderer = new Renderer(m_ecs, m_window->getWindowHandle(), m_window->getWidth(), m_window->getHeight());
	m_physics = new Physics(m_ecs);
	m_animationSystem = new AnimationSystem(m_ecs);
	m_userInput = new UserInput(*m_window, m_ecs);
	AssetManager::init();

	AssetHandlerRegistration::registerHandlers(m_renderer, m_physics, m_animationSystem);

	ImGuiInputAdapter imguiInputAdapter(ImGui::GetCurrentContext(), *m_userInput, *m_window);
	imguiInputAdapter.resize(m_window->getWidth(), m_window->getHeight(), m_window->getWindowWidth(), m_window->getWindowHeight());

	CharacterMovementSystem characterMovementSystem(m_ecs);

	m_gameLogic->init(this);

	Timer timer;

	constexpr float k_stepSize = 1.0f / 60.0f;
	float accumulator = k_stepSize; // make sure we simulate in the first frame

	while (!m_window->shouldClose())
	{
		PROFILING_FRAME_MARK;

		timer.update();
		float timeDelta = fminf(0.5f, static_cast<float>(timer.getTimeDelta()));



		accumulator += timeDelta;
		while (accumulator >= k_stepSize)
		{
			PROFILING_ZONE_SCOPED_N("Simulation");

			accumulator -= k_stepSize;


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

			m_physics->update(k_stepSize);

			m_gameLogic->update(k_stepSize);

			characterMovementSystem.update(k_stepSize);

			m_animationSystem->update(k_stepSize);

			ImGui::Render();
		}

		m_renderer->render();
	}

	m_gameLogic->shutdown();

	delete m_level;

	AssetHandlerRegistration::unregisterHandlers();
	AssetManager::shutdown();

	delete m_userInput;
	delete m_animationSystem;
	delete m_physics;
	delete m_renderer;
	delete m_ecs;
	delete m_window;

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

//UserInput *Engine::getUserInput() noexcept
//{
//	return m_userInput;
//}

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
