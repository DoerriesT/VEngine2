#include "Camera.h"
#include <glm/gtx/transform.hpp>
#include "component/TransformComponent.h"
#include "component/CameraComponent.h"
#include "TransformHierarchy.h"

Camera::Camera(TransformComponent &transformComponent, CameraComponent &cameraComponent) noexcept
	:m_transformComponent(transformComponent),
	m_cameraComponent(cameraComponent)
{
	updateViewMatrix();
	updateProjectionMatrix();
}

void Camera::setRotation(const glm::quat &rotation) noexcept
{
	m_transformComponent.m_transform.m_rotation = rotation;
	m_transformComponent.m_globalTransform.m_rotation = rotation;
	updateViewMatrix();
}

void Camera::setPosition(const glm::vec3 &position) noexcept
{
	m_transformComponent.m_transform.m_translation = position;
	m_transformComponent.m_globalTransform.m_translation = position;
	updateViewMatrix();
}

void Camera::setAspectRatio(float aspectRatio) noexcept
{
	m_cameraComponent.m_aspectRatio = aspectRatio;
	updateProjectionMatrix();
}

void Camera::setFovy(float fovy) noexcept
{
	m_cameraComponent.m_fovy = fovy;
	updateProjectionMatrix();
}

void Camera::rotate(const glm::vec3 &pitchYawRollOffset) noexcept
{
	glm::quat tmp = glm::quat(glm::vec3(-pitchYawRollOffset.x, 0.0f, 0.0f));
	glm::quat tmp1 = glm::quat(glm::angleAxis(-pitchYawRollOffset.y, glm::vec3(0.0f, 1.0f, 0.0f)));
	m_transformComponent.m_transform.m_rotation = glm::normalize(tmp1 * m_transformComponent.m_transform.m_rotation * tmp);
	m_transformComponent.m_globalTransform.m_rotation = m_transformComponent.m_transform.m_rotation;

	updateViewMatrix();
}

void Camera::translate(const glm::vec3 &translationOffset) noexcept
{
	glm::vec3 strafe(m_cameraComponent.m_viewMatrix[0][0], m_cameraComponent.m_viewMatrix[1][0], m_cameraComponent.m_viewMatrix[2][0]);
	glm::vec3 up(m_cameraComponent.m_viewMatrix[0][1], m_cameraComponent.m_viewMatrix[1][1], m_cameraComponent.m_viewMatrix[2][1]);
	glm::vec3 forward(m_cameraComponent.m_viewMatrix[0][2], m_cameraComponent.m_viewMatrix[1][2], m_cameraComponent.m_viewMatrix[2][2]);

	m_transformComponent.m_transform.m_translation += translationOffset.z * forward + translationOffset.x * strafe + translationOffset.y * up;
	m_transformComponent.m_globalTransform.m_translation = m_transformComponent.m_transform.m_translation;
	updateViewMatrix();
}

void Camera::lookAt(const glm::vec3 &targetPosition)
{
	constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);

	m_cameraComponent.m_viewMatrix = glm::lookAt(m_transformComponent.m_transform.m_translation, targetPosition, up);
	m_transformComponent.m_transform.m_rotation = glm::inverse(glm::quat_cast(m_cameraComponent.m_viewMatrix));
	m_transformComponent.m_globalTransform.m_rotation = m_transformComponent.m_transform.m_rotation;
}

glm::mat4 Camera::getViewMatrix() const noexcept
{
	return m_cameraComponent.m_viewMatrix;
}

glm::mat4 Camera::getProjectionMatrix() const noexcept
{
	return m_cameraComponent.m_projectionMatrix;
}

glm::vec3 Camera::getPosition() const noexcept
{
	return m_transformComponent.m_transform.m_translation;
}

glm::quat Camera::getRotation() const noexcept
{
	return m_transformComponent.m_transform.m_rotation;
}

glm::vec3 Camera::getForwardDirection() const noexcept
{
	return -glm::vec3(m_cameraComponent.m_viewMatrix[0][2], m_cameraComponent.m_viewMatrix[1][2], m_cameraComponent.m_viewMatrix[2][2]);
}

glm::vec3 Camera::getUpDirection() const noexcept
{
	return glm::vec3(m_cameraComponent.m_viewMatrix[0][1], m_cameraComponent.m_viewMatrix[1][1], m_cameraComponent.m_viewMatrix[2][1]);
}

glm::vec3 Camera::getRightDirection() const noexcept
{
	return glm::vec3(m_cameraComponent.m_viewMatrix[0][0], m_cameraComponent.m_viewMatrix[1][0], m_cameraComponent.m_viewMatrix[2][0]);
}

float Camera::getAspectRatio() const noexcept
{
	return m_cameraComponent.m_aspectRatio;
}

float Camera::getFovy() const noexcept
{
	return m_cameraComponent.m_fovy;
}

void Camera::updateViewMatrix() noexcept
{
	m_cameraComponent.m_viewMatrix = glm::mat4_cast(glm::inverse(m_transformComponent.m_transform.m_rotation)) * glm::translate(-m_transformComponent.m_transform.m_translation);
}

void Camera::updateProjectionMatrix() noexcept
{
	m_cameraComponent.m_projectionMatrix = glm::perspective((m_cameraComponent.m_fovy), m_cameraComponent.m_aspectRatio, m_cameraComponent.m_far, m_cameraComponent.m_near);
}
