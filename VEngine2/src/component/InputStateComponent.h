#pragma once

struct InputStateAction
{
	bool m_pressed : 1;
	bool m_down : 1;
	bool m_released : 1;
};

struct InputStateComponent
{
	float m_moveForwardAxis;
	float m_moveRightAxis;
	float m_turnRightAxis;
	float m_lookUpAxis;
	float m_zoomAxis;
	InputStateAction m_jumpAction;
	InputStateAction m_crouchAction;
	InputStateAction m_sprintAction;
	InputStateAction m_shootAction;
};