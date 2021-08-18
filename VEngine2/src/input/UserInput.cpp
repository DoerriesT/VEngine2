#include "UserInput.h"
#include "Utility/ContainerUtility.h"
#include "Window/Window.h"
#include "ecs/ECS.h"
#include "component/RawInputStateComponent.h"
#include "component/InputStateComponent.h"

UserInput::UserInput(Window &window, ECS *ecs)
	:m_window(window),
	m_ecs(ecs)
{
	m_window.addInputListener(this);
}

void UserInput::resize(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height)
{
	m_offsetX = offsetX;
	m_offsetY = offsetY;
	m_width = width;
	m_height = height;
}

void UserInput::input()
{
	// update raw input state component
	{
		auto *state = m_ecs->getSingletonComponent<RawInputStateComponent>();

		// update indices
		state->m_prevIndex = state->m_curIndex;
		state->m_curIndex = (state->m_curIndex + 1) % 2;

		// update gamepads
		state->m_gamepadCount = 0;
		if (m_gamepads)
		{
			memcpy(state->m_gamepadStates[state->m_curIndex], m_gamepads, sizeof(m_gamepads[0]) * m_gamepadCount);
			state->m_gamepadCount = m_gamepadCount;
		}

		// update mouse and delta
		state->m_mousePosX[state->m_curIndex] = m_mousePos.x;
		state->m_mousePosY[state->m_curIndex] = m_mousePos.y;
		state->m_mousePosDeltaX = state->m_mousePosX[state->m_curIndex] - state->m_mousePosX[state->m_prevIndex];
		state->m_mousePosDeltaY = state->m_mousePosY[state->m_curIndex] - state->m_mousePosY[state->m_prevIndex];
		state->m_scrollDeltaX = m_scrollOffset.x;
		state->m_scrollDeltaY = m_scrollOffset.y;

		// update key state
		state->m_pressedKeys[state->m_curIndex] = m_pressedKeys;
		state->m_pressedMouseButtons[state->m_curIndex] = m_pressedMouseButtons;

		// set clipboard
		state->m_clipboardString = m_window.getClipboardText();
	}

	// update input state component
	{
		auto *rawState = m_ecs->getSingletonComponent<RawInputStateComponent>();
		auto *state = m_ecs->getSingletonComponent<InputStateComponent>();

		*state = {};

		// mouse + keyboard
		state->m_moveForwardAxis += rawState->isKeyDown(InputKey::W) ? 1.0f : 0.0f;
		state->m_moveForwardAxis += rawState->isKeyDown(InputKey::S) ? -1.0f : 0.0f;
		state->m_moveRightAxis += rawState->isKeyDown(InputKey::D) ? 1.0f : 0.0f;
		state->m_moveRightAxis += rawState->isKeyDown(InputKey::A) ? -1.0f : 0.0f;
		state->m_turnRightAxis += rawState->isMouseButtonDown(InputMouse::BUTTON_RIGHT) ? rawState->m_mousePosDeltaX * 0.005f : 0.0f;
		state->m_lookUpAxis += rawState->isMouseButtonDown(InputMouse::BUTTON_RIGHT) ? -rawState->m_mousePosDeltaY * 0.005f : 0.0f;
		state->m_zoomAxis += -rawState->m_scrollDeltaY;
		state->m_jumpAction = { rawState->isKeyPressed(InputKey::SPACE), rawState->isKeyDown(InputKey::SPACE), rawState->isKeyReleased(InputKey::SPACE) };
		state->m_crouchAction = { rawState->isKeyPressed(InputKey::LEFT_CONTROL), rawState->isKeyDown(InputKey::LEFT_CONTROL), rawState->isKeyReleased(InputKey::LEFT_CONTROL) };
		state->m_sprintAction = { rawState->isKeyPressed(InputKey::LEFT_SHIFT), rawState->isKeyDown(InputKey::LEFT_SHIFT), rawState->isKeyReleased(InputKey::LEFT_SHIFT) };
		state->m_shootAction = { rawState->isKeyPressed(InputKey::F), rawState->isKeyDown(InputKey::F), rawState->isKeyReleased(InputKey::F) };


		// gamepad
		if (rawState->m_gamepadCount > 0 && rawState->m_gamepadStates[rawState->m_curIndex][0].m_id != -1)
		{
			const auto &gp = rawState->m_gamepadStates[rawState->m_curIndex][0];
			const auto &gpPrev = rawState->m_gamepadStates[rawState->m_prevIndex][0];
			state->m_moveForwardAxis += gp.m_leftStickY * (float)(!gp.m_rightStick);
			state->m_moveRightAxis += gp.m_leftStickX * (float)(!gp.m_rightStick);
			state->m_turnRightAxis += gp.m_rightStickX * 0.05f * (float)(!gp.m_rightStick);
			state->m_lookUpAxis += gp.m_rightStickY * 0.05f * (float)(!gp.m_rightStick);
			state->m_zoomAxis += -gp.m_leftStickY * 0.2f * (float)(gp.m_rightStick);
			state->m_jumpAction = { gp.m_buttonA && !gpPrev.m_buttonA, gp.m_buttonA, !gp.m_buttonA && gpPrev.m_buttonA };
			//state->m_crouchAction = { rawState->isKeyPressed(InputKey::LEFT_CONTROL), rawState->isKeyDown(InputKey::LEFT_CONTROL), rawState->isKeyReleased(InputKey::LEFT_CONTROL) };
			state->m_sprintAction = { gp.m_buttonB && !gpPrev.m_buttonB, gp.m_buttonB, !gp.m_buttonB && gpPrev.m_buttonB };
			state->m_shootAction = { gp.m_buttonX && !gpPrev.m_buttonX, gp.m_buttonX, !gp.m_buttonX && gpPrev.m_buttonX };
		}
	}

	m_mousePosDelta = (m_mousePos - m_previousMousePos);
	m_previousMousePos = m_mousePos;
	m_scrollOffset = {};
}

glm::vec2 UserInput::getPreviousMousePos() const
{
	return m_previousMousePos;
}

glm::vec2 UserInput::getCurrentMousePos() const
{
	return m_mousePos;
}

glm::vec2 UserInput::getMousePosDelta() const
{
	return m_mousePosDelta;
}

glm::vec2 UserInput::getScrollOffset() const
{
	return m_scrollOffset;
}

void UserInput::setMousePos(float x, float y)
{
	if (m_width != 0 || m_height != 0)
	{
		x += m_offsetX;
		y += m_offsetY;
	}

	m_window.setCursorPos(x, y);
}

const char *UserInput::getClipboardText() const
{
	return m_window.getClipboardText();
}

void UserInput::setClipboardText(const char *text)
{
	m_window.setClipboardText(text);
}

bool UserInput::isKeyPressed(InputKey key, bool ignoreRepeated) const
{
	size_t pos = static_cast<size_t>(key);
	return pos < m_repeatedKeys.size() && pos < m_repeatedKeys.size() && m_pressedKeys[pos] && (!ignoreRepeated || !m_repeatedKeys[pos]);
}

bool UserInput::isMouseButtonPressed(InputMouse mouseButton) const
{
	return m_pressedMouseButtons[static_cast<size_t>(mouseButton)];
}

void UserInput::addKeyListener(IKeyListener *listener)
{
	m_keyListeners.push_back(listener);
}

void UserInput::removeKeyListener(IKeyListener *listener)
{
	m_keyListeners.erase(std::remove(m_keyListeners.begin(), m_keyListeners.end(), listener), m_keyListeners.end());
}

void UserInput::addCharListener(ICharListener *listener)
{
	m_charListeners.push_back(listener);
}

void UserInput::removeCharListener(ICharListener *listener)
{
	m_charListeners.erase(std::remove(m_charListeners.begin(), m_charListeners.end(), listener), m_charListeners.end());
}

void UserInput::addScrollListener(IScrollListener *listener)
{
	m_scrollListeners.push_back(listener);
}

void UserInput::removeScrollListener(IScrollListener *listener)
{
	m_scrollListeners.erase(std::remove(m_scrollListeners.begin(), m_scrollListeners.end(), listener), m_scrollListeners.end());
}

void UserInput::addMouseButtonListener(IMouseButtonListener *listener)
{
	m_mouseButtonlisteners.push_back(listener);
}

void UserInput::removeMouseButtonListener(IMouseButtonListener *listener)
{
	m_mouseButtonlisteners.erase(std::remove(m_mouseButtonlisteners.begin(), m_mouseButtonlisteners.end(), listener), m_mouseButtonlisteners.end());
}

void UserInput::onKey(InputKey key, InputAction action)
{
	for (IKeyListener *listener : m_keyListeners)
	{
		listener->onKey(key, action);
	}

	const auto keyIndex = static_cast<size_t>(key);

	switch (action)
	{
	case InputAction::RELEASE:
		if (keyIndex < m_pressedKeys.size() && keyIndex < m_repeatedKeys.size())
		{
			m_pressedKeys.set(static_cast<size_t>(key), false);
			m_repeatedKeys.set(static_cast<size_t>(key), false);
		}
		break;
	case InputAction::PRESS:
		if (keyIndex < m_pressedKeys.size())
		{
			m_pressedKeys.set(static_cast<size_t>(key), true);
		}
		break;
	case InputAction::REPEAT:
		if (keyIndex < m_repeatedKeys.size())
		{
			m_repeatedKeys.set(static_cast<size_t>(key), true);
		}
		break;
	default:
		break;
	}
}

void UserInput::onChar(Codepoint charKey)
{
	for (ICharListener *listener : m_charListeners)
	{
		listener->onChar(charKey);
	}
}

void UserInput::onMouseButton(InputMouse mouseButton, InputAction action)
{
	for (IMouseButtonListener *listener : m_mouseButtonlisteners)
	{
		listener->onMouseButton(mouseButton, action);
	}

	if (action == InputAction::RELEASE)
	{
		m_pressedMouseButtons.set(static_cast<size_t>(mouseButton), false);
	}
	else if (action == InputAction::PRESS)
	{
		m_pressedMouseButtons.set(static_cast<size_t>(mouseButton), true);
	}
}

void UserInput::onMouseMove(double x, double y)
{
	m_mousePos.x = static_cast<float>(x);
	m_mousePos.y = static_cast<float>(y);

	if (m_width != 0 || m_height != 0)
	{
		m_mousePos.x -= m_offsetX;
		m_mousePos.y -= m_offsetY;
	}
}

void UserInput::onMouseScroll(double xOffset, double yOffset)
{
	for (IScrollListener *listener : m_scrollListeners)
	{
		listener->onMouseScroll(xOffset, yOffset);
	}

	m_scrollOffset.x = static_cast<float>(xOffset);
	m_scrollOffset.y = static_cast<float>(yOffset);
}

void UserInput::onGamepadStateUpdate(size_t count, const GamepadState *gamepads)
{
	m_gamepadCount = count;
	m_gamepads = gamepads;
}
