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
	Shader* modelShader = new Shader("Shaders/SPIR-V/vertModelIns.spv", "Shaders/SPIR-V/fragModel.spv");
	m_renderer->RegisterShader(modelShader);

	Shader* rectShader = new Shader("Shaders/SPIR-V/vertRect.spv", "Shaders/SPIR-V/fragRect.spv");
	m_renderer->RegisterShader(rectShader);

	Texture* testTexture = new Texture(m_renderer, "Assets/Objects/Metal/diffuse.tga");
	Texture* testTexture2 = new Texture(m_renderer, "Assets/Objects/Metal/normal.tga");

	Material* testMat = new Material(m_renderer, modelShader, { testTexture, testTexture2 });

	//Mesh* testMesh = new Mesh(m_renderer, "Assets/Objects/Soulspear/soulspear.obj");
	//Mesh* testMesh = new Mesh(m_renderer, "Assets/Objects/Stanford/Dragon.obj");
	Mesh* testMesh = new Mesh(m_renderer, "Assets/Primitives/sphere.obj");

	MeshRenderer* testObject = new MeshRenderer(m_renderer, testMesh, testMat, &MeshRenderer::m_defaultInstanceAttributes, 100);

	float fDeltaTime = 0.0f;	
	float fDebugDisplayTime = DEBUG_DISPLAY_TIME;

	Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), 0.3f, 5.0f);

	m_renderer->AddDynamicObject(testObject);
	glm::mat4 instanceModelMat;

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

		// Draw...
		m_renderer->Begin();

		/*
		if (m_input->GetKey(GLFW_KEY_G) && !m_input->GetKey(GLFW_KEY_G, INPUTSTATE_PREVIOUS)) 
		{
			std::cout << "Adding object!" << std::endl;

			instanceModelMat = glm::translate(instanceModelMat, glm::vec3(0.0f, 0.0f, -3.0f));

			Instance newInstance = { instanceModelMat };
			testObject->AddInstance(newInstance);
		}
		*/
		
		if (m_input->GetKey(GLFW_KEY_G))
		{
			instanceModelMat = glm::translate(instanceModelMat, glm::vec3(0.0f, 0.0f, -3.0f * fDeltaTime));

			Instance newInstance = { instanceModelMat };
			testObject->SetInstance(0, newInstance);
		}

		glm::mat4 viewMat = camera.GetViewMatrix();

		m_renderer->SetViewMatrix(viewMat);

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

	delete testMesh;

	delete testMat;

	m_renderer->UnregisterShader(modelShader);
	delete modelShader;

	m_renderer->UnregisterShader(rectShader);
	delete rectShader;
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
