#include "Input.h"
#include <memory>

Input* Input::m_instance = nullptr;

Input::Input()
{
	m_currentState = new char[512];
	m_prevState = new char[512];

	m_currentMouseState = new MouseState[8];
	m_prevMouseState = new MouseState[8];

	// Set initial data...
	memset(m_currentMouseState->m_buttons, -1, sizeof(char) * 8);
	memset(m_prevMouseState->m_buttons, -1, sizeof(char) * 8);

	m_currentMouseState->m_fMouseAxes[0] = 0.0;
	m_currentMouseState->m_fMouseAxes[1] = 0.0;
	m_prevMouseState->m_fMouseAxes[0] = 0.0;
	m_prevMouseState->m_fMouseAxes[1] = 0.0;

	m_currentMouseState->m_fMouseAxes[2] = 0.0;
	m_currentMouseState->m_fMouseAxes[3] = 0.0;
	m_prevMouseState->m_fMouseAxes[2] = 0.0;
	m_prevMouseState->m_fMouseAxes[3] = 0.0;

	m_keyStates[0] = m_currentState;
	m_keyStates[1] = m_prevState;

	m_mouseStates[0] = m_currentMouseState;
	m_mouseStates[1] = m_prevMouseState;

	// Wipe both input states to 0. (Garbage memory is not always 0.)
	memset(m_currentState, 0, sizeof(char) * 512);
	memset(m_prevState, 0, sizeof(char) * 512);
}

Input::~Input()
{
	delete[] m_prevState;
	delete[] m_currentState;

	delete m_prevMouseState;
	delete m_currentMouseState;
}

char* Input::GetCurrentState() 
{
	return m_currentState;
}

MouseState* Input::GetCurrentMouseState() 
{
	return m_currentMouseState;
}

int Input::GetKey(int keyCode, EInputState state)
{
	// Return the keycode state of the selected state.
	return m_keyStates[state][keyCode];
}

int Input::GetMouseButton(EMouseButton button, EInputState state)
{
	// Return the mouse button state of selected state.
	return m_mouseStates[state]->m_buttons[button] + 1;
}

float Input::GetCursorX(EInputState state)
{
	return static_cast<float>(m_mouseStates[state]->m_fMouseAxes[0]);
}

float Input::GetCursorY(EInputState state)
{
	return static_cast<float>(m_mouseStates[state]->m_fMouseAxes[1]);
}

float Input::GetScrollX(EInputState state) 
{
	return static_cast<float>(m_mouseStates[state]->m_fMouseAxes[2]);
}

float Input::GetScrollY(EInputState state) 
{
	return static_cast<float>(m_mouseStates[state]->m_fMouseAxes[3]);
}

void Input::EndFrame()
{
	// Copy current state to previous state.
	memcpy_s(m_prevState, sizeof(char) * 512, m_currentState, sizeof(char) * 512);

	// Copy current mouse state to previous mouse state.
	memcpy_s(m_prevMouseState, sizeof(MouseState), m_currentMouseState, sizeof(MouseState));

	// Reset scroll values.
	memset(&m_currentMouseState->m_fMouseAxes[2], 0, sizeof(double) * 2);
}

void Input::Create() 
{
	if (!m_instance)
		m_instance = new Input;
}

void Input::Destroy() 
{
	if (m_instance)
		delete m_instance;
}

Input* Input::GetInstance() 
{
	return m_instance;
}
