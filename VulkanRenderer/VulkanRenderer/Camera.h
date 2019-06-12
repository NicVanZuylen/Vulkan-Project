#pragma once
#include "glm.hpp"
#include "glm\include\gtc\quaternion.hpp"
#include "glm/include/ext.hpp"

class Input;

struct GLFWwindow;

class Camera
{
public:

	Camera();

	Camera(glm::vec3 v3Position, glm::vec3 v3EulerAngles, float fSensitivity = 0.1f, float fMovespeed = 5.0f);

	~Camera();

	/*
	Description: Update the camera input and movement this frame.
	Param:
	    float fDeltaTime: Time between frames.
		Input* input: Pointer to the input class of the application.
		GLFWwindow* window: Pointer to the window instance.
	*/
	void Update(float fDeltaTime, Input* input, GLFWwindow* window);

	/*
	Description: Get the worldspace position of the camera.
	Return Type: vec3
	*/
	glm::vec3 GetPosition();

	/*
	Description: Get the worldspace model matrix of the camera.
	Return Type: mat4
	*/
	glm::mat4 GetWorldMatrix();

	/*
	Description: Get the view matrix of this camera.
	Return Type: mat4
	*/
	glm::mat4 GetViewMatrix();

private:

	float m_fSensitivity;
	float m_fMovespeed;
	glm::vec3 m_v3Position;
	glm::vec3 m_v3EulerAngles;
	glm::mat4 m_matrix;

	float m_fLastMouseX;
	float m_fLastMouseY;

	bool m_bLooking;
};

