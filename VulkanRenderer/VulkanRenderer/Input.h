#pragma once

enum EInputState 
{
    INPUTSTATE_CURRENT,
	INPUTSTATE_PREVIOUS
};

enum EMouseButton 
{
	MOUSEBUTTON_LEFT,
	MOUSEBUTTON_RIGHT,
	MOUSEBUTTON_MIDDLE,
	MOUSEBUTTON_3,
	MOUSEBUTTON_4,
	MOUSEBUTTON_5,
	MOUSEBUTTON_6,
	MOUSEBUTTON_7
};

struct MouseState 
{
	char m_buttons[8];
	double m_fMouseAxes[4];
};

class Input
{
public:

	Input();

	~Input();

	/*
	Description: Get the raw keyboard state data this frame.
	Return Type: char
	*/
	char* GetCurrentState();

	/*
	Description: Get the raw mouse/cursor state data this frame.
	Return Type: MouseState*
	*/
	MouseState* GetCurrentMouseState();

	/*
	Description: Get the input state of the specified key.
	Return Type: int
	Param:
	    int keycode: The keycode of the key to read (Most keys in ASCII).
	    EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
	*/
	int GetKey(int keyCode, EInputState state = INPUTSTATE_CURRENT);

	/*
	Description: Get the input state of the specified mouse button.
	Return Type: int
	Param:
	    EMouseButton button: The button to read.
	    EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
	*/
	int GetMouseButton(EMouseButton button, EInputState state = INPUTSTATE_CURRENT);

	/*
	Description: Get the X coordinate of the cursor.
	Return Type: float
	Param:
	    EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
	*/
	float GetCursorX(EInputState state = INPUTSTATE_CURRENT);

	/*
	Description: Get the Y coordinate of the cursor.
	Return Type: float
	Param:
	    EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
	*/
	float GetCursorY(EInputState state = INPUTSTATE_CURRENT);

	/*
	Description: Get the X scroll value this frame.
	Return Type: float
	Param:
	    EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
	*/
	float GetScrollX(EInputState state = INPUTSTATE_CURRENT);
	
	/*
	Description: Get the Y scroll value this frame.
	Return Type: float
	Param:
	    EInputState state: The input state to read. (Can be INPUTSTATE_CURRENT or INPUTSTATE_PREVIOUS.)
	*/
	float GetScrollY(EInputState state = INPUTSTATE_CURRENT);

	/*
	Description: Swaps the current keyboard state to the previous keyboard state.
	*/
	void EndFrame();

	// Singleton functions.

	static void Create();
	static void Destroy();
	static Input* GetInstance();

private:

	static Input* m_instance;

	char* m_keyStates[2];
	MouseState* m_mouseStates[2];

	char* m_currentState;
	char* m_prevState;

	MouseState* m_currentMouseState;
	MouseState* m_prevMouseState;
};

