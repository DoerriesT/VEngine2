#pragma once
#include <stdint.h>
#include "IInputListener.h"
#include "utility/Timer.h"

struct ImGuiContext;

class UserInput;
class Window;

class ImGuiInputAdapter : public IInputListener
{
public:
	explicit ImGuiInputAdapter(ImGuiContext *context, UserInput &input, Window &window) noexcept;
	void update() noexcept;
	void resize(uint32_t width, uint32_t height, uint32_t windowWidth, uint32_t windowHeight) noexcept;
	void onKey(InputKey key, InputAction action) noexcept override;
	void onChar(Codepoint charKey) override;
	void onMouseButton(InputMouse mouseButton, InputAction action) noexcept override;
	void onMouseMove(double x, double y) noexcept override;
	void onMouseScroll(double xOffset, double yOffset) noexcept override;
	void onGamepadStateUpdate(size_t count, const GamepadState *gamepads) noexcept override;

private:
	ImGuiContext *m_context = nullptr;
	UserInput &m_input;
	Window &m_window;
	Timer m_timer;
	bool m_mouseJustPressed[8] = {};
	uint32_t m_width = 1;
	uint32_t m_height = 1;
	uint32_t m_windowWidth = 1;
	uint32_t m_windowHeight = 1;
};