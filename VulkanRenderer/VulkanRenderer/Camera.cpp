#include "Camera.h"
#include "Input.h"
#include "glfw3.h"

#include <iostream>

Camera::Camera()
{
	m_fSensitivity = 0.1f;
	m_fMovespeed = 5.0f;
	m_v3Position = { 0.0f, 0.0f, 0.0f };
	m_v3EulerAngles = { 0.0f, 0.0f, 0.0f };
	m_bLooking = false;

	m_fLastMouseX = 0.0f;
	m_fLastMouseY = 0.0f;
}

Camera::Camera(glm::vec3 v3Position, glm::vec3 v3EulerAngles, float fSensitivity, float fMovespeed) 
{
	m_fSensitivity = fSensitivity;
	m_fMovespeed = fMovespeed;
	m_bLooking = false;
	m_v3Position = v3Position;
	m_v3EulerAngles = v3EulerAngles;

	m_fLastMouseX = 0.0f;
	m_fLastMouseY = 0.0f;

	// Create translation and rotation matricies.
	glm::mat4 posMat = glm::translate(glm::mat4(), v3Position);
	glm::mat4 rotMat = glm::rotate(glm::mat4(), m_v3EulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
	rotMat *= glm::rotate(rotMat, m_v3EulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
	rotMat *= glm::rotate(rotMat, m_v3EulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));
	
	// The camera will rotate around a pivot at it's centre, so concatenate translation first and rotation second.
	m_matrix = posMat * rotMat;
}

Camera::~Camera()
{

}

void Camera::Update(float fDeltaTime, Input* input, GLFWwindow* window) 
{
	float fNewMouseX = input->GetCursorX();
	float fNewMouseY = input->GetCursorY();

	// Look
	if (input->GetMouseButton(MOUSEBUTTON_RIGHT))
	{
		float fXDiff = fNewMouseX - m_fLastMouseX;
		float fYDiff = fNewMouseY - m_fLastMouseY;

		if(!m_bLooking) 
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			m_bLooking = true;
		}

		m_v3EulerAngles.y -= fXDiff * m_fSensitivity * fDeltaTime;
		m_v3EulerAngles.x -= fYDiff * m_fSensitivity * fDeltaTime;
	}
	else if(m_bLooking)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		m_bLooking = false;
	}

	// Strafe
	if (input->GetKey(GLFW_KEY_W))
	{
		glm::vec3 forwardVec = -m_matrix[2];

		m_v3Position += forwardVec * m_fMovespeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_A))
	{
		glm::vec3 leftVec = -m_matrix[0];

		m_v3Position += leftVec * m_fMovespeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_S))
	{
		glm::vec3 backVec = m_matrix[2];

		m_v3Position += backVec * m_fMovespeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_D))
	{
		glm::vec3 rightVec = m_matrix[0];

		m_v3Position += rightVec * m_fMovespeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_SPACE))
	{
		glm::vec3 upVec = m_matrix[1];

		m_v3Position += upVec * m_fMovespeed * fDeltaTime;
	}
	if (input->GetKey(GLFW_KEY_LEFT_CONTROL))
	{
		glm::vec3 downVec = -m_matrix[1];

		m_v3Position += downVec * m_fMovespeed * fDeltaTime;
	}

	// Construct translation and rotation matrices...
	glm::mat4 posMat = glm::translate(glm::mat4(), m_v3Position);
	glm::mat4 rotMat = glm::rotate(glm::mat4(), m_v3EulerAngles.z, glm::vec3(0.0f, 0.0f, 1.0f));
	rotMat *= glm::rotate(rotMat, m_v3EulerAngles.y, glm::vec3(0.0f, 1.0f, 0.0f));
	rotMat *= glm::rotate(rotMat, m_v3EulerAngles.x, glm::vec3(1.0f, 0.0f, 0.0f));

	// The camera will rotate around a pivot at it's centre, so concatenate translation first and rotation second.
	m_matrix = posMat * rotMat;

	m_fLastMouseX = fNewMouseX;
	m_fLastMouseY = fNewMouseY;
}

glm::vec3 Camera::GetPosition() 
{
	return m_matrix[3];
}

glm::mat4 Camera::GetWorldMatrix() 
{
	return m_matrix;
}

glm::mat4 Camera::GetViewMatrix() 
{
	return glm::inverse(m_matrix);
}
