#include "ImGuiInputAdapter.h"
#include "graphics/imgui/imgui.h"
#include "InputTokens.h"
#include "UserInput.h"
#include "window/Window.h"

static const char *getClipboardText(void *userData)
{
	return ((UserInput *)userData)->getClipboardText();
}

static void setClipboardText(void *userData, const char *text)
{
	((UserInput *)userData)->setClipboardText(text);
}

ImGuiInputAdapter::ImGuiInputAdapter(ImGuiContext *context, UserInput &input, Window &window) noexcept
	:m_context(context),
	m_input(input),
	m_window(window)
{
	// make context current
	auto *prevImGuiContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_context);

	m_timer.reset();

	// Setup back-end capabilities flags
	ImGuiIO &io = ImGui::GetIO();
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
	io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
#if 0
	io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)
#endif
	io.BackendPlatformName = "ImGuiInputAdapter";

	// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
	io.KeyMap[ImGuiKey_Tab] = (int)InputKey::TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = (int)InputKey::LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = (int)InputKey::RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = (int)InputKey::UP;
	io.KeyMap[ImGuiKey_DownArrow] = (int)InputKey::DOWN;
	io.KeyMap[ImGuiKey_PageUp] = (int)InputKey::PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = (int)InputKey::PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = (int)InputKey::HOME;
	io.KeyMap[ImGuiKey_End] = (int)InputKey::END;
	io.KeyMap[ImGuiKey_Insert] = (int)InputKey::INSERT;
	io.KeyMap[ImGuiKey_Delete] = (int)InputKey::DELETE;
	io.KeyMap[ImGuiKey_Backspace] = (int)InputKey::BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = (int)InputKey::SPACE;
	io.KeyMap[ImGuiKey_Enter] = (int)InputKey::ENTER;
	io.KeyMap[ImGuiKey_Escape] = (int)InputKey::ESCAPE;
	io.KeyMap[ImGuiKey_KeyPadEnter] = (int)InputKey::KP_ENTER;
	io.KeyMap[ImGuiKey_A] = (int)InputKey::A;
	io.KeyMap[ImGuiKey_C] = (int)InputKey::C;
	io.KeyMap[ImGuiKey_V] = (int)InputKey::V;
	io.KeyMap[ImGuiKey_X] = (int)InputKey::X;
	io.KeyMap[ImGuiKey_Y] = (int)InputKey::Y;
	io.KeyMap[ImGuiKey_Z] = (int)InputKey::Z;

	io.SetClipboardTextFn = setClipboardText;
	io.GetClipboardTextFn = getClipboardText;
	io.ClipboardUserData = &input;

	// Our mouse update function expect PlatformHandle to be filled for the main viewport
	ImGuiViewport *main_viewport = ImGui::GetMainViewport();
	main_viewport->PlatformHandle = window.getWindowHandle();
	main_viewport->PlatformHandleRaw = window.getWindowHandleRaw();

	// restore context
	ImGui::SetCurrentContext(prevImGuiContext);

	m_input.addCharListener(this);
	m_input.addKeyListener(this);
	m_input.addMouseButtonListener(this);
	m_input.addScrollListener(this);
}

void ImGuiInputAdapter::update() noexcept
{
	// make context current
	auto *prevImGuiContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_context);

	ImGuiIO &io = ImGui::GetIO();
	IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer back-end. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

	// Setup display size (every frame to accommodate for window resizing
	io.DisplaySize = ImVec2((float)m_windowWidth, (float)m_windowHeight);
	if (m_width > 0 && m_height > 0)
	{
		io.DisplayFramebufferScale = ImVec2((float)m_width / m_windowWidth, (float)m_height / m_windowHeight);
	}

	// Setup time step
	m_timer.update();
	float timeDelta = (float)m_timer.getTimeDelta();
	io.DeltaTime = timeDelta > 0.0f ? timeDelta : (1.0f / 60.0f);

	// update mouse pos and buttons
	{
		// Update buttons
		for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
		{
			// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
			io.MouseDown[i] = m_mouseJustPressed[i] || m_window.getMouseButton((InputMouse)i) != InputAction::RELEASE;
			m_mouseJustPressed[i] = false;
		}

		// Update mouse position
		const ImVec2 mouse_pos_backup = io.MousePos;
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
		io.MouseHoveredViewport = 0;
		ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
		for (int n = 0; n < platform_io.Viewports.Size; n++)
		{
			ImGuiViewport *viewport = platform_io.Viewports[n];

			if (m_window.isFocused())
			{
				if (io.WantSetMousePos)
				{
					m_input.setMousePos((mouse_pos_backup.x - viewport->Pos.x), (mouse_pos_backup.y - viewport->Pos.y));
				}
				else
				{
					auto mousePos = m_input.getCurrentMousePos();
					// Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
					io.MousePos = ImVec2(mousePos.x, mousePos.y);
				}
				for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
				{
					io.MouseDown[i] |= m_window.getMouseButton((InputMouse)i) != InputAction::RELEASE;
				}
			}
		}
	}

	// update mouse cursor
	if (!((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || m_window.getMouseCursorMode() == Window::MouseCursorMode::DISABLED))
	{
		ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
		ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
		for (int n = 0; n < platform_io.Viewports.Size; n++)
		{
			GLFWwindow *window = (GLFWwindow *)platform_io.Viewports[n]->PlatformHandle;
			if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
			{
				// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
				m_window.setMouseCursorMode(Window::MouseCursorMode::HIDDEN);
			}
			else
			{
				Window::MouseCursor mouseCursor = Window::MouseCursor::ARROW;
				// Show OS mouse cursor
				// FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
				switch (imgui_cursor)
				{
				case ImGuiMouseCursor_None:
					mouseCursor = Window::MouseCursor::ARROW;
					break;
				case ImGuiMouseCursor_Arrow:
					mouseCursor = Window::MouseCursor::ARROW;
					break;
				case ImGuiMouseCursor_TextInput:
					mouseCursor = Window::MouseCursor::TEXT;
					break;
				case ImGuiMouseCursor_ResizeAll:
					mouseCursor = Window::MouseCursor::RESIZE_ALL;
					break;
				case ImGuiMouseCursor_ResizeNS:
					mouseCursor = Window::MouseCursor::RESIZE_VERTICAL;
					break;
				case ImGuiMouseCursor_ResizeEW:
					mouseCursor = Window::MouseCursor::RESIZE_HORIZONTAL;
					break;
				case ImGuiMouseCursor_ResizeNESW:
					mouseCursor = Window::MouseCursor::RESIZE_TRBL;
					break;
				case ImGuiMouseCursor_ResizeNWSE:
					mouseCursor = Window::MouseCursor::RESIZE_TLBR;
					break;
				case ImGuiMouseCursor_Hand:
					mouseCursor = Window::MouseCursor::HAND;
					break;
				default:
					assert(false);
					break;
				}
				m_window.setMouseCursor(mouseCursor);
				m_window.setMouseCursorMode(Window::MouseCursorMode::NORMAL);
			}
		}
	}

	// Update game controllers (if enabled and available)
	memset(io.NavInputs, 0, sizeof(io.NavInputs));
#if 0
	if (!((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0))
	{
		// Update gamepad inputs
#define MAP_BUTTON(NAV_NO, BUTTON_NO)       { if (buttons_count > BUTTON_NO && buttons[BUTTON_NO] == GLFW_PRESS) io.NavInputs[NAV_NO] = 1.0f; }
#define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1) { float v = (axes_count > AXIS_NO) ? axes[AXIS_NO] : V0; v = (v - V0) / (V1 - V0); if (v > 1.0f) v = 1.0f; if (io.NavInputs[NAV_NO] < v) io.NavInputs[NAV_NO] = v; }
		int axes_count = 0, buttons_count = 0;
		const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
		const unsigned char *buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
		MAP_BUTTON(ImGuiNavInput_Activate, 0);     // Cross / A
		MAP_BUTTON(ImGuiNavInput_Cancel, 1);     // Circle / B
		MAP_BUTTON(ImGuiNavInput_Menu, 2);     // Square / X
		MAP_BUTTON(ImGuiNavInput_Input, 3);     // Triangle / Y
		MAP_BUTTON(ImGuiNavInput_DpadLeft, 13);    // D-Pad Left
		MAP_BUTTON(ImGuiNavInput_DpadRight, 11);    // D-Pad Right
		MAP_BUTTON(ImGuiNavInput_DpadUp, 10);    // D-Pad Up
		MAP_BUTTON(ImGuiNavInput_DpadDown, 12);    // D-Pad Down
		MAP_BUTTON(ImGuiNavInput_FocusPrev, 4);     // L1 / LB
		MAP_BUTTON(ImGuiNavInput_FocusNext, 5);     // R1 / RB
		MAP_BUTTON(ImGuiNavInput_TweakSlow, 4);     // L1 / LB
		MAP_BUTTON(ImGuiNavInput_TweakFast, 5);     // R1 / RB
		MAP_ANALOG(ImGuiNavInput_LStickLeft, 0, -0.3f, -0.9f);
		MAP_ANALOG(ImGuiNavInput_LStickRight, 0, +0.3f, +0.9f);
		MAP_ANALOG(ImGuiNavInput_LStickUp, 1, +0.3f, +0.9f);
		MAP_ANALOG(ImGuiNavInput_LStickDown, 1, -0.3f, -0.9f);
#undef MAP_BUTTON
#undef MAP_ANALOG
		if (axes_count > 0 && buttons_count > 0)
			io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
		else
			io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
	}
#endif

	// restore context
	ImGui::SetCurrentContext(prevImGuiContext);
}

void ImGuiInputAdapter::resize(uint32_t width, uint32_t height, uint32_t windowWidth, uint32_t windowHeight) noexcept
{
	m_width = width;
	m_height = height;
	m_windowWidth = windowWidth;
	m_windowHeight = windowHeight;
}

void ImGuiInputAdapter::onKey(InputKey key, InputAction action) noexcept
{
	// make context current
	auto *prevImGuiContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_context);

	ImGuiIO &io = ImGui::GetIO();
	if (action == InputAction::PRESS)
		io.KeysDown[(int)key] = true;
	if (action == InputAction::RELEASE)
		io.KeysDown[(int)key] = false;

	// Modifiers are not reliable across systems
	io.KeyCtrl = io.KeysDown[(int)InputKey::LEFT_CONTROL] || io.KeysDown[(int)InputKey::RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[(int)InputKey::LEFT_SHIFT] || io.KeysDown[(int)InputKey::RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[(int)InputKey::LEFT_ALT] || io.KeysDown[(int)InputKey::RIGHT_ALT];
	io.KeySuper = io.KeysDown[(int)InputKey::LEFT_SUPER] || io.KeysDown[(int)InputKey::RIGHT_SUPER];

	// restore context
	ImGui::SetCurrentContext(prevImGuiContext);
}

void ImGuiInputAdapter::onChar(Codepoint charKey)
{
	// make context current
	auto *prevImGuiContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_context);

	ImGuiIO &io = ImGui::GetIO();
	io.AddInputCharacter(charKey);

	// restore context
	ImGui::SetCurrentContext(prevImGuiContext);
}

void ImGuiInputAdapter::onMouseButton(InputMouse mouseButton, InputAction action) noexcept
{
	// make context current
	auto *prevImGuiContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_context);

	size_t button = (size_t)mouseButton;
	if (action == InputAction::PRESS && button >= 0 && button < IM_ARRAYSIZE(m_mouseJustPressed))
	{
		m_mouseJustPressed[button] = true;
	}

	// restore context
	ImGui::SetCurrentContext(prevImGuiContext);
}

void ImGuiInputAdapter::onMouseMove(double x, double y) noexcept
{
}

void ImGuiInputAdapter::onMouseScroll(double xOffset, double yOffset) noexcept
{
	// make context current
	auto *prevImGuiContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_context);

	ImGuiIO &io = ImGui::GetIO();
	io.MouseWheelH += (float)xOffset;
	io.MouseWheel += (float)yOffset;

	// restore context
	ImGui::SetCurrentContext(prevImGuiContext);
}
