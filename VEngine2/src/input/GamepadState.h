#pragma once

struct GamepadState
{
	int m_id;
	float m_leftStickX;
	float m_leftStickY;
	float m_rightStickX;
	float m_rightStickY;
	float m_leftTrigger;
	float m_rightTrigger;
	bool m_buttonA : 1;
	bool m_buttonB : 1;
	bool m_buttonX : 1;
	bool m_buttonY : 1;
	bool m_dPadUp : 1;
	bool m_dPadRight : 1;
	bool m_dPadDown : 1;
	bool m_dPadLeft : 1;
	bool m_leftButton : 1;
	bool m_rightButton : 1;
	bool m_backButton : 1;
	bool m_startButton : 1;
	bool m_leftStick : 1;
	bool m_rightStick : 1;
};