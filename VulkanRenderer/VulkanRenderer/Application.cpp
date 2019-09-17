#include "Application.h"
#include "Renderer.h"
#include "Input.h"

#include "glfw3.h"

#include <iostream>
#include <chrono>

#include "glm.hpp"
#include "glm\include\gtc\quaternion.hpp"
#include "glm/include/ext.hpp"

#include "Shader.h"
#include "VertexInfo.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "MeshRenderer.h"

#include "Camera.h"

GLFWwindow* Application::m_window = nullptr;
Renderer* Application::m_renderer = nullptr;
Input* Application::m_input = nullptr;
bool Application::m_bFullScreen = false;
bool Application::m_bGLFWInit = false;

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
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// Create window.
	CreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, m_bFullScreen);

	// Create renderer.
	m_renderer = new Renderer(m_window);

	// Initialize input.
	Input::Create();
	m_input = Input::GetInstance();

	return 0;
}

void Application::Run() 
{
	Shader* modelShader = new Shader(m_renderer, "Shaders/SPIR-V/vertModelInsG.spv", "Shaders/SPIR-V/fragModelG.spv");

	Texture* testTexture = new Texture(m_renderer, "Assets/Objects/Metal/diffuse.tga");
	Texture* testTexture2 = new Texture(m_renderer, "Assets/Objects/Metal/normal.tga");

	Material* testMat = new Material(m_renderer, modelShader, { testTexture, testTexture2 });

	Mesh* testMesh = new Mesh(m_renderer, "Assets/Objects/Stanford/Dragon.obj");
	Mesh* testMesh2 = new Mesh(m_renderer, "Assets/Primitives/sphere.obj");

	MeshRenderer* testObject = new MeshRenderer(m_renderer, testMesh, testMat, &MeshRenderer::m_defaultInstanceAttributes, 100);
	MeshRenderer* testObject2 = new MeshRenderer(m_renderer, testMesh2, testMat, &MeshRenderer::m_defaultInstanceAttributes, 100);

	float fDeltaTime = 0.0f;	
	float fDebugDisplayTime = DEBUG_DISPLAY_TIME;

	Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), 0.3f, 5.0f);
	
	Instance ins;
	ins.m_modelMat = glm::translate(glm::vec3(0.0f, 0.0f, 5.0f));

	testObject2->SetInstance(0, ins);

	glm::mat4 instanceModelMat;

	glm::mat4 moveModelMat;

	m_renderer->UpdateDirectionalLight(glm::vec4(0.0f, 0.0f, -1.0f, 1.0f), glm::vec4(1.0f), 0);
	m_renderer->UpdateDirectionalLight(glm::vec4(0.0f, -1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), 1);

	m_renderer->AddPointLight(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), 15.0f);
	m_renderer->AddPointLight(glm::vec4(-3.0f, 0.0f, 3.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), 7.0f);

	while(!glfwWindowShouldClose(m_window)) 
	{
		// Time
		auto startTime = std::chrono::high_resolution_clock::now();

		// Quit if escape is pressed.
		if (m_input->GetKey(GLFW_KEY_ESCAPE))
			glfwSetWindowShouldClose(m_window, 1);

		// ------------------------------------------------------------------------------------

		fDebugDisplayTime -= fDeltaTime;

		camera.Update(fDeltaTime, m_input, m_window);

		// Poll events.
		glfwPollEvents();

		// Fullscreen
		if (m_input->GetKey(GLFW_KEY_F11) && !m_input->GetKey(GLFW_KEY_F11, INPUTSTATE_PREVIOUS))
		{
			// Get window's monitor and video mode.
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);

			m_bFullScreen = !m_bFullScreen;

			// Recreate window.
			if(m_bFullScreen)
			    CreateWindow(vidMode->width, vidMode->height, true);
			else
				CreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, false);

			// Set new window for the renderer, it will also re-create the swap chain and graphics pipelines to accomodate.
			m_renderer->SetWindow(m_window, vidMode->width, vidMode->height);

			m_input->ResetStates();
		}

		// Draw...
		m_renderer->Begin();
		
		if (m_input->GetKey(GLFW_KEY_G) && !m_input->GetKey(GLFW_KEY_G, INPUTSTATE_PREVIOUS)) 
		{
			std::cout << "Adding object!" << std::endl;

			instanceModelMat = glm::translate(instanceModelMat, glm::vec3(0.0f, 0.0f, -3.0f));

			Instance newInstance = { instanceModelMat };
			testObject->AddInstance(newInstance);
		}

		if (m_input->GetKey(GLFW_KEY_M))
		{
			moveModelMat = glm::translate(moveModelMat, glm::vec3(0.0f, 0.0f, -3.0f * fDeltaTime));

			Instance newInstance = { moveModelMat };
			testObject->SetInstance(0, newInstance);
		}

		glm::mat4 viewMat = camera.GetViewMatrix();
		glm::vec3 v4ViewPos = camera.GetPosition();

		m_renderer->SetViewMatrix(viewMat, v4ViewPos);

		m_renderer->End();

		m_input->EndFrame();

		// End time...
		auto endTime = std::chrono::high_resolution_clock::now();

		auto timeDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

		fDeltaTime = static_cast<float>(timeDuration) / 1000000.0f;

		if(fDebugDisplayTime <= 0.0f) 
		{
			std::cout << "Frametime: " << fDeltaTime * 1000.0f << "ms\n";
			std::cout << "FPS: " << (int)ceilf((1.0f / fDeltaTime)) << std::endl;

			fDebugDisplayTime = DEBUG_DISPLAY_TIME;
		}
	}


	delete testTexture2;
	delete testTexture;

	delete testObject;
	delete testObject2;

	delete testMesh;
	delete testMesh2;

	delete testMat;

	//m_renderer->UnregisterShader(modelShader);
	delete modelShader;
}

void Application::CreateWindow(const unsigned int& nWidth, const unsigned int& nHeight, bool bFullScreen)
{
	if (m_window)
		glfwDestroyWindow(m_window);

	// Create window.
	if(bFullScreen) 
	{
		m_window = glfwCreateWindow(nWidth, nHeight, "Vulkan Test", glfwGetPrimaryMonitor(), 0);
	}
	else 
	{
		m_window = glfwCreateWindow(nWidth, nHeight, "Vulkan Test", 0, 0);
	}

	// Set key callback...
	glfwSetKeyCallback(m_window, &KeyCallBack);

	// Set mouse callbacks...
	glfwSetMouseButtonCallback(m_window, &MouseButtonCallBack);
	glfwSetCursorPosCallback(m_window, &CursorPosCallBack);
	glfwSetScrollCallback(m_window, &MouseScrollCallBack);

	// Set window resize callback.
	glfwSetFramebufferSizeCallback(m_window, &WindowResizeCallback);
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

void Application::WindowResizeCallback(GLFWwindow* window, int nWidth, int nHeight) 
{
	m_renderer->ResizeWindow(nWidth, nHeight, true);
}
