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
#include "RenderObject.h"
#include "SubScene.h"
#include "LightingManager.h"

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
	Scene* scene = m_renderer->GetScene();
	SubScene* subScene = scene->GetPrimarySubScene();

	// Load shaders
	Shader* modelShader = new Shader(m_renderer, "Shaders/SPIR-V/model_pbr_vert.spv", "Shaders/SPIR-V/model_pbr_frag.spv");

	// Load textures.
	Texture* spearDiffuse = new Texture(m_renderer, "Assets/Objects/Soulspear/soulspear_diffuse.tga");
	Texture* spearNormal = new Texture(m_renderer, "Assets/Objects/Soulspear/soulspear_normal.tga");
	Texture* spearSpecular = new Texture(m_renderer, "Assets/Objects/Soulspear/soulspear_specular.tga");

	Texture* floorTex = new Texture(m_renderer, "Assets/Objects/Metal/diffuse.tga");
	Texture* floorTex2 = new Texture(m_renderer, "Assets/Objects/Metal/normal.tga");
	Texture* floorTex3 = new Texture(m_renderer, "Assets/Objects/Metal/glow.tga");
	//Texture* testTexture = new Texture(m_renderer, "Assets/Objects/Viking Tiles/Base_basecolor.tga");
	//Texture* testTexture2 = new Texture(m_renderer, "Assets/Objects/Viking Tiles/Base_normal.tga");

	// Construct materials
	//Material* testMat = new Material
	//(
	//	m_renderer, 
	//	modelShader, 
	//	{ testTexture, testTexture2, testTexture3 },
	//	{}
	//);
	Material* spearMat = new Material
	(
		m_renderer,
		modelShader,
		{ spearDiffuse, spearNormal, spearSpecular },
		{ { MATERIAL_PROPERTY_FLOAT, "_Roughness" } }
	);
	spearMat->SetFloat4("_ColorTint", glm::value_ptr(glm::vec4(1.0f)));
	spearMat->SetFloat("_Roughness", 1.0f);

	std::cout << spearMat->GetFloat("_Roughness") << "\n";

	Material* floorMat = new Material(m_renderer, modelShader, { floorTex, floorTex2, floorTex3 }, {});

	// Load meshes.
	Mesh* planeMesh = new Mesh(m_renderer, "Assets/Primitives/plane.obj");
	Mesh* spearMesh = new Mesh(m_renderer, "Assets/Objects/Soulspear/soulspear.obj");
	//Mesh* spinnerDetailsMesh = new Mesh(m_renderer, "Assets/Objects/Spinner/low_details.obj");
	//Mesh* spinnerGlassMesh = new Mesh(m_renderer, "Assets/Objects/Spinner/low_glass.obj");
	//Mesh* spinnerPaintMesh = new Mesh(m_renderer, "Assets/Objects/Spinner/low_paint.obj");

	// Create render objects.
	RenderObject* floorObj = new RenderObject(scene, planeMesh, floorMat, &RenderObject::m_defaultInstanceAttributes, 1);

	//RenderObject* spinnerDetailsObj = new RenderObject(scene, spinnerDetailsMesh, spearMat, &RenderObject::m_defaultInstanceAttributes, 10);
	//RenderObject* spinnerGlassObj = new RenderObject(scene, spinnerGlassMesh, spearMat, &RenderObject::m_defaultInstanceAttributes, 10);
	//RenderObject* spinnerPaintObj = new RenderObject(scene, spinnerPaintMesh, spearMat, &RenderObject::m_defaultInstanceAttributes, 10);
	
	RenderObject* spearObj = new RenderObject(scene, spearMesh, spearMat, &RenderObject::m_defaultInstanceAttributes, 10U);

	// Time variables.
	float fDeltaTime = 0.0f;	
	float fDebugDisplayTime = DEBUG_DISPLAY_TIME;
	float fElapsedTime = 0.0f;

	Camera camera(glm::vec3(0.0f, 3.0f, 10.0f), glm::vec3(0.0f), 0.3f, 5.0f);
	
	Instance ins;

	// Scale down spinner instances.
	ins.m_modelMat = glm::scale(glm::mat4(), glm::vec3(0.01f, 0.01f, 0.01f));

	//spinnerDetailsObj->SetInstance(0, ins);
	//spinnerGlassObj->SetInstance(0, ins);
	//spinnerPaintObj->SetInstance(0, ins);

	ins.m_modelMat = glm::mat4();
	floorObj->SetInstance(0, ins);

	// Model matrix for new object instances.
	glm::mat4 instanceModelMat;

	LightingManager* lightManager = subScene->GetLightingManager();

	// Update directional lights.
	lightManager->AddDirLight({ glm::normalize(glm::vec4(0.0f, -1.0f, 1.0f, 0.0f)), glm::vec4(1.0f) });
	//lightManager->AddDirLight({ glm::normalize(glm::vec4(0.0f, -1.0f, 1.0f, 0.0f)), glm::vec4(1.0f, 0.8f, 0.5f, 1.0f) });
	//lightManager->AddDirLight({ glm::vec4(0.0f, -1.0f, 0.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) });

	// Add point lights to the scene.
	//lightManager->AddPointLight({ glm::vec4(0.0f, 3.0f, 5.0f, 1.0f), glm::vec3(1.0f), 10.0f });
	//lightManager->AddPointLight({ glm::vec4(-3.0f, 3.5f, 3.5f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), 3.0f });
	//lightManager->AddPointLight({ glm::vec4(-5.0f, 2.0f, 5.0f, 1.0f), glm::vec3(1.0f), 5.0f });

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
			// Get primary monitor and video mode.
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);

			m_bFullScreen = !m_bFullScreen;

			// Recreate window.
			if (m_bFullScreen) 
			{
			    CreateWindow(vidMode->width, vidMode->height, true);

			    // Set new window for the renderer, it will also re-create the swap chain and graphics pipelines to accomodate.
			    m_renderer->SetWindow(m_window, vidMode->width, vidMode->height);
			}
			else 
			{
				CreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, false);

				// Set new window for the renderer, it will also re-create the swap chain and graphics pipelines to accomodate.
				m_renderer->SetWindow(m_window, WINDOW_WIDTH, WINDOW_HEIGHT);
			}

			m_input->ResetStates();
		}

		// Rotate spinner model.
		glm::mat4 spinnerScaleMat = glm::scale(glm::mat4(), glm::vec3(1.0f));
		ins.m_modelMat = glm::rotate(spinnerScaleMat, -fElapsedTime, glm::vec3(0.0f, 1.0f, 0.0f));
		spearObj->SetInstance(0, ins);
		//spinnerDetailsObj->SetInstance(0, ins);
		//spinnerGlassObj->SetInstance(0, ins);
		//spinnerPaintObj->SetInstance(0, ins);

		// Draw...
		m_renderer->Begin();
		
		// Add an instance of a model to the scene if G is pressed.
		if (m_input->GetKey(GLFW_KEY_G) && !m_input->GetKey(GLFW_KEY_G, INPUTSTATE_PREVIOUS)) 
		{
			std::cout << "Adding object!" << std::endl;

			instanceModelMat = glm::translate(instanceModelMat, glm::vec3(-3.0f, 0.0f, 0.0f));

			Instance newInstance = { instanceModelMat * glm::scale(glm::mat4(), glm::vec3(0.01f)) };
			spearObj->AddInstance(newInstance);
			//spinnerDetailsObj->AddInstance(newInstance);
			//spinnerGlassObj->AddInstance(newInstance);
			//spinnerPaintObj->AddInstance(newInstance);
		}

		glm::mat4 viewMat = camera.GetViewMatrix();
		glm::vec3 v4ViewPos = camera.GetPosition();

		// Set rendering view matrix.
		//m_renderer->SetViewMatrix(viewMat, v4ViewPos);
		subScene->UpdateCameraView(viewMat, glm::vec4(v4ViewPos, 1.0f));

		// End frame.
		m_renderer->End();

		m_input->EndFrame();

		// End time...
		std::chrono::steady_clock::time_point endTime;
		long long timeDuration;

		// Reset deltatime.
		fDeltaTime = 0.0f;

		// Framerate limitation...
		// Wait for deltatime to reach value based upon frame cap.
		while(fDeltaTime < (1000.0f / FRAMERATE_CAP) / 1000.0f) 
		{
			endTime = std::chrono::high_resolution_clock::now();
			timeDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

			fDeltaTime = static_cast<float>(timeDuration) / 1000000.0f;
		}

		// Get deltatime and add to elapsed time.
		fElapsedTime += fDeltaTime;

		// Display frametime and FPS.
		if(fDebugDisplayTime <= 0.0f) 
		{
			std::cout << "Frametime: " << fDeltaTime * 1000.0f << "ms\n";
			std::cout << "Elapsed Time: " << fElapsedTime << "s\n";
			std::cout << "FPS: " << (int)ceilf((1.0f / fDeltaTime)) << "\n";

			fDebugDisplayTime = DEBUG_DISPLAY_TIME;
		}
	}

	delete floorTex3;
	delete floorTex2;
	delete floorTex;

	delete spearDiffuse;
	delete spearNormal;
	delete spearSpecular;

	delete floorObj;

	//delete spinnerDetailsObj;
	//delete spinnerGlassObj;
	//delete spinnerPaintObj;
	delete spearObj;

	delete planeMesh;
	delete spearMesh;

	//delete spinnerDetailsMesh;
	//delete spinnerGlassMesh;
	//delete spinnerPaintMesh;

	delete spearMat;
	delete floorMat;

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
