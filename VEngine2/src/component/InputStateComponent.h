#pragma once
#include <EASTL/bitset.h>
#include "input/InputTokens.h"

struct RawInputStateComponent
{
	float m_mousePosX[2] = {};
	float m_mousePosY[2] = {};
	float m_mousePosDeltaX = 0.0f;
	float m_mousePosDeltaY = 0.0f;
	float m_scrollDeltaX = 0.0f;
	float m_scrollDeltaY = 0.0f;
	eastl::bitset<350> m_pressedKeys[2] = {};
	eastl::bitset<8> m_pressedMouseButtons[2] = {};
	const char *m_clipboardString = nullptr;
	size_t m_curIndex = 0;
	size_t m_prevIndex = 1;

	/// <summary>
	/// Check if the key is down. While isKeyPressed() only returns true on the first frame the key is down, 
	/// this function returns true until the key is released.
	/// </summary>
	/// <param name="key">The key to check.</param>
	/// <returns>True if the key is currently down.</returns>
	inline bool isKeyDown(InputKey key) const noexcept
	{
		return m_pressedKeys[m_curIndex][static_cast<size_t>(key)];
	}

	/// <summary>
	/// Check if the key was pressed this frame. While isKeyDown() returns true until the key is released,
	/// this function only returns true on the first frame the key is down.
	/// </summary>
	/// <param name="key">The key to check.</param>
	/// <returns>True if the key is pressed.</returns>
	inline bool isKeyPressed(InputKey key) const noexcept
	{
		const size_t idx = static_cast<size_t>(key);
		return m_pressedKeys[m_curIndex][idx] && !m_pressedKeys[m_prevIndex][idx];
	}

	/// <summary>
	/// Check if the key was released this frame.
	/// </summary>
	/// <param name="key">The key to check.</param>
	/// <returns>True if the key was released this frame.</returns>
	inline bool isKeyReleased(InputKey key) const noexcept
	{
		const size_t idx = static_cast<size_t>(key);
		return !m_pressedKeys[m_curIndex][idx] && m_pressedKeys[m_prevIndex][idx];
	}

	/// <summary>
	/// Check if the mouse button is down. While isMouseButtonPressed() only returns true on the first frame the mouse button is down, 
	/// this function returns true until the mouse button is released.
	/// </summary>
	/// <param name="button">The mouse button to check.</param>
	/// <returns>True if the mouse button is currently down.</returns>
	inline bool isMouseButtonDown(InputMouse button) const noexcept
	{
		return m_pressedMouseButtons[m_curIndex][static_cast<size_t>(button)];
	}

	/// <summary>
	/// Check if the mouse button was pressed this frame. While isMouseButtonDown() returns true until the mouse button is released,
	/// this function only returns true on the first frame the mouse button is down.
	/// </summary>
	/// <param name="button">The mouse button to check.</param>
	/// <returns>True if the mouse button is pressed.</returns>
	inline bool isMouseButtonPressed(InputMouse button) const noexcept
	{
		const size_t idx = static_cast<size_t>(button);
		return m_pressedMouseButtons[m_curIndex][idx] && !m_pressedMouseButtons[m_prevIndex][idx];
	}

	/// <summary>
	/// Check if the mouse button was released this frame.
	/// </summary>
	/// <param name="button">The mouse button to check.</param>
	/// <returns>True if the mouse button is pressed.</returns>
	inline bool isMouseButtonReleased(InputMouse button) const noexcept
	{
		const size_t idx = static_cast<size_t>(button);
		return !m_pressedMouseButtons[m_curIndex][idx] && m_pressedMouseButtons[m_prevIndex][idx];
	}
};