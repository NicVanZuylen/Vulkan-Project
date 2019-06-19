#include "Application.h"
#include "Renderer.h"
#include "Input.h"

#include "glfw3.h"

#include <iostream>
#include <chrono>

#include "glm.hpp"
#include "glm\include\gtc\quaternion.hpp"
#include "glm/include/ext.hpp"

Input* Application::m_input = nullptr;

Application::Application()
{

}

Application::~Application()
{
	if (m_bGLFWInit)
		glfwTerminate();
	else
		return;

	// Destroy renderer.
	delete m_renderer;

	// Destroy window.
	glfwDestroyWindow(m_window);

	// Destroy input.
	Input::Destroy();
}

int Application::Init() 
{
	m_bGLFWInit = false;

	if (!glfwInit())
		return -1;

	m_bGLFWInit = true;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// Create window.
	m_window = glfwCreateWindow(1280, 720, "Vulkan Test", 0, 0);

	// Create renderer.
	m_renderer = new Renderer(m_window);

	// Set key callback...
	glfwSetKeyCallback(m_window, &KeyCallBack);

	// Set mouse callbacks...
	glfwSetMouseButtonCallback(m_window, &MouseButtonCallBack);
	glfwSetCursorPosCallback(m_window, &CursorPosCallBack);
	glfwSetScrollCallback(m_window, &MouseScrollCallBack);

	// Initialize input.
	Input::Create();
	m_input = Input::GetInstance();

	return 0;
}

void Application::Run() 
{
	float fDeltaTime = 0.0f;	

	while(!glfwWindowShouldClose(m_window)) 
	{
		// Time
		auto startTime = std::chrono::high_resolution_clock::now();

		// Quit if escape is pressed.
		if (m_input->GetKey(GLFW_KEY_ESCAPE))
			glfwSetWindowShouldClose(m_window, 1);

		// ------------------------------------------------------------------------------------

		// Poll events.
		glfwPollEvents();

		// Draw...
		m_renderer->DrawFrame();

		// End time...
		auto endTime = std::chrono::high_resolution_clock::now();

		auto timeDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

		fDeltaTime = static_cast<float>(timeDuration) / 1000000.0f;
	}
}

void Application::ErrorCallBack(int error, const char* desc)
{
	std::cout << "GLFW Error: " << desc << "\n";
}

void Application::KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	m_input->GetCurrentState()[key] = action;
}

void Application::MouseButtonCallBack(GLFWwindow* window, int button, int action, int mods)
{
	m_input->GetCurrentMouseState()->m_buttons[button] = action - 1;
}

void Application::CursorPosCallBack(GLFWwindow* window, double dXPos, double dYPos)
{
	MouseState* currentState = m_input->GetCurrentMouseState();

	currentState->m_fMouseAxes[0] = dXPos;
	currentState->m_fMouseAxes[1] = dYPos;
}

void Application::MouseScrollCallBack(GLFWwindow* window, double dXOffset, double dYOffset)
{
	MouseState* currentState = Input::GetInstance()->GetCurrentMouseState();

	currentState->m_fMouseAxes[2] = dXOffset;
	currentState->m_fMouseAxes[3] = dYOffset;
}
