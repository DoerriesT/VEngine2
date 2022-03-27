#include "Camera.h"
#include <glm/gtx/transform.hpp>
#include "component/TransformComponent.h"
#include "component/CameraComponent.h"
#include "TransformHierarchy.h"
#include "ecs/ECS.h"

Camera::Camera(const Transform &transform, float aspectRatio, float fovy, float nearPlane, float farPlane) noexcept
	:m_transform(transform),
	m_aspectRatio(aspectRatio),
	m_fovy(fovy),
	m_nearPlane(nearPlane),
	m_farPlane(farPlane)
{
	updateViewMatrix();
	updateProjectionMatrix();
}

void Camera::setRotation(const glm::quat &rotation) noexcept
{
	m_transform.m_rotation = rotation;
	updateViewMatrix();
}

void Camera::setPosition(const glm::vec3 &position) noexcept
{
	m_transform.m_translation = position;
	updateViewMatrix();
}

void Camera::setAspectRatio(float aspectRatio) noexcept
{
	m_aspectRatio = aspectRatio;
	updateProjectionMatrix();
}

void Camera::setFovy(float fovy) noexcept
{
	m_fovy = fovy;
	updateProjectionMatrix();
}

void Camera::setNearPlane(float nearPlane) noexcept
{
	m_nearPlane = nearPlane;
	updateProjectionMatrix();
}

void Camera::setFarPlane(float farPlane) noexcept
{
	m_farPlane = farPlane;
	updateProjectionMatrix();
}

void Camera::rotate(const glm::vec3 &pitchYawRollOffset) noexcept
{
	glm::quat tmp = glm::quat(glm::vec3(-pitchYawRollOffset.x, 0.0f, 0.0f));
	glm::quat tmp1 = glm::quat(glm::angleAxis(-pitchYawRollOffset.y, glm::vec3(0.0f, 1.0f, 0.0f)));
	m_transform.m_rotation = glm::normalize(tmp1 * m_transform.m_rotation * tmp);

	updateViewMatrix();
}

void Camera::translate(const glm::vec3 &translationOffset) noexcept
{
	glm::vec3 strafe(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]);
	glm::vec3 up(m_viewMatrix[0][1], m_viewMatrix[1][1], m_viewMatrix[2][1]);
	glm::vec3 forward(m_viewMatrix[0][2], m_viewMatrix[1][2], m_viewMatrix[2][2]);

	m_transform.m_translation += translationOffset.z * forward + translationOffset.x * strafe + translationOffset.y * up;
	updateViewMatrix();
}

void Camera::lookAt(const glm::vec3 &targetPosition)
{
	constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);

	m_viewMatrix = glm::lookAt(m_transform.m_translation, targetPosition, up);
	m_transform.m_rotation = glm::inverse(glm::quat_cast(m_viewMatrix));
}

Transform Camera::getTransform() const noexcept
{
	return m_transform;
}

glm::mat4 Camera::getViewMatrix() const noexcept
{
	return m_viewMatrix;
}

glm::mat4 Camera::getProjectionMatrix() const noexcept
{
	return m_projectionMatrix;
}

glm::vec3 Camera::getPosition() const noexcept
{
	return m_transform.m_translation;
}

glm::quat Camera::getRotation() const noexcept
{
	return m_transform.m_rotation;
}

glm::vec3 Camera::getForwardDirection() const noexcept
{
	return -glm::vec3(m_viewMatrix[0][2], m_viewMatrix[1][2], m_viewMatrix[2][2]);
}

glm::vec3 Camera::getUpDirection() const noexcept
{
	return glm::vec3(m_viewMatrix[0][1], m_viewMatrix[1][1], m_viewMatrix[2][1]);
}

glm::vec3 Camera::getRightDirection() const noexcept
{
	return glm::vec3(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]);
}

float Camera::getAspectRatio() const noexcept
{
	return m_aspectRatio;
}

float Camera::getFovy() const noexcept
{
	return m_fovy;
}

float Camera::getNearPlane() const noexcept
{
	return m_nearPlane;
}

float Camera::getFarPlane() const noexcept
{
	return m_farPlane;
}

void Camera::updateViewMatrix() noexcept
{
	m_viewMatrix = glm::mat4_cast(glm::inverse(m_transform.m_rotation)) * glm::translate(-m_transform.m_translation);
}

void Camera::updateProjectionMatrix() noexcept
{
	m_projectionMatrix = glm::perspective(m_fovy, m_aspectRatio, m_farPlane, m_nearPlane);
}

Camera CameraECSAdapter::createFromComponents(const TransformComponent *tc, const CameraComponent *cc) noexcept
{
	return Camera(tc->m_globalTransform, cc->m_aspectRatio, cc->m_fovy, cc->m_near, cc->m_far);
}

void CameraECSAdapter::updateComponents(const Camera &camera, ECS *ecs, EntityID entity) noexcept
{
	TransformHierarchy::setGlobalTransform(ecs, entity, camera.getTransform());
	auto *cc = ecs->getComponent<CameraComponent>(entity);
	assert(cc);
	cc->m_aspectRatio = camera.getAspectRatio();
	cc->m_fovy = camera.getFovy();
	cc->m_near = camera.getNearPlane();
	cc->m_far = camera.getFarPlane();
	cc->m_viewMatrix = camera.getViewMatrix();
	cc->m_projectionMatrix = camera.getProjectionMatrix();
}
