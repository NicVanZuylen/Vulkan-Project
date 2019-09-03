#pragma once

struct GLFWwindow;

class Renderer;
class Input;

#define DEBUG_DISPLAY_TIME 2.0f

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

class Application
{
public:

	Application();

	~Application();

	int Init();

	void Run();

private:

	static void CreateWindow(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen = false);

	// GLFW Callbacks
	static void ErrorCallBack(int error, const char* desc);
	static void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void MouseButtonCallBack(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallBack(GLFWwindow* window, double dXPos, double dYPos);
	static void MouseScrollCallBack(GLFWwindow* window, double dXOffset, double dYOffset);
	static void WindowResizeCallback(GLFWwindow* window, int nWidth, int nHeight);

	static GLFWwindow* m_window;
	static Renderer* m_renderer;
	static Input* m_input;
	static bool m_bFullScreen;
	static bool m_bGLFWInit;
};

