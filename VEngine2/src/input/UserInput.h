#pragma once
#include "IInputListener.h"
#include <glm/vec2.hpp>
#include <EASTL/vector.h>
#include <EASTL/bitset.h>

class Window;
class ECS;

class UserInput : public IInputListener
{
public:
	explicit UserInput(Window &window, ECS *ecs);
	UserInput(const UserInput &) = delete;
	UserInput(const UserInput &&) = delete;
	UserInput &operator= (const UserInput &) = delete;
	UserInput &operator= (const UserInput &&) = delete;
	void resize(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height);
	void input();
	glm::vec2 getPreviousMousePos() const;
	glm::vec2 getCurrentMousePos() const;
	glm::vec2 getMousePosDelta() const;
	glm::vec2 getScrollOffset() const;
	void setMousePos(float x, float y);
	const char *getClipboardText() const;
	void setClipboardText(const char *text);
	bool isKeyPressed(InputKey key, bool ignoreRepeated = false) const;
	bool isMouseButtonPressed(InputMouse mouseButton) const;
	void addKeyListener(IKeyListener *listener);
	void removeKeyListener(IKeyListener *listener);
	void addCharListener(ICharListener *listener);
	void removeCharListener(ICharListener *listener);
	void addScrollListener(IScrollListener *listener);
	void removeScrollListener(IScrollListener *listener);
	void addMouseButtonListener(IMouseButtonListener *listener);
	void removeMouseButtonListener(IMouseButtonListener *listener);
	void onKey(InputKey key, InputAction action) override;
	void onChar(Codepoint charKey) override;
	void onMouseButton(InputMouse mouseButton, InputAction action) override;
	void onMouseMove(double x, double y) override;
	void onMouseScroll(double xOffset, double yOffset) override;
	void onGamepadStateUpdate(size_t count, const GamepadState *gamepads);

private:
	Window &m_window;
	ECS *m_ecs = nullptr;
	int32_t m_offsetX = 0;
	int32_t m_offsetY = 0;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	glm::vec2 m_mousePos;
	glm::vec2 m_previousMousePos;
	glm::vec2 m_mousePosDelta;
	glm::vec2 m_scrollOffset;
	eastl::vector<IKeyListener *> m_keyListeners;
	eastl::vector<ICharListener *> m_charListeners;
	eastl::vector<IScrollListener *> m_scrollListeners;
	eastl::vector<IMouseButtonListener *> m_mouseButtonlisteners;
	eastl::bitset<350> m_pressedKeys;
	eastl::bitset<350> m_repeatedKeys;
	eastl::bitset<8> m_pressedMouseButtons;
	size_t m_gamepadCount = 0;
	const GamepadState *m_gamepads = nullptr;
};