#pragma once
#include "utility/Transform.h"
#include "ecs/ECSCommon.h"

class ECS;
struct CameraComponent;
struct TransformComponent;

class Camera
{
public:
	explicit Camera() noexcept = default;
	explicit Camera(const Transform &transform, float aspectRatio, float fovy, float nearPlane, float farPlane) noexcept;
	void setRotation(const glm::quat &rotation) noexcept;
	void setPosition(const glm::vec3 &position) noexcept;
	void setAspectRatio(float aspectRatio) noexcept;
	void setFovy(float fovy) noexcept;
	void setNearPlane(float nearPlane) noexcept;
	void setFarPlane(float farPlane) noexcept;
	void rotate(const glm::vec3 &pitchYawRollOffset) noexcept;
	void translate(const glm::vec3 &translationOffset) noexcept;
	void lookAt(const glm::vec3 &targetPosition);
	Transform getTransform() const noexcept;
	glm::mat4 getViewMatrix() const noexcept;
	glm::mat4 getProjectionMatrix() const noexcept;
	glm::vec3 getPosition() const noexcept;
	glm::quat getRotation() const noexcept;
	glm::vec3 getForwardDirection() const noexcept;
	glm::vec3 getUpDirection() const noexcept;
	glm::vec3 getRightDirection() const noexcept;
	float getAspectRatio() const noexcept;
	float getFovy() const noexcept;
	float getNearPlane() const noexcept;
	float getFarPlane() const noexcept;

private:
	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;
	Transform m_transform;
	float m_aspectRatio = 1.0f;
	float m_fovy = glm::radians(90.0f);
	float m_nearPlane = 0.1f;
	float m_farPlane = 1024.0f;

	void updateViewMatrix() noexcept;
	void updateProjectionMatrix() noexcept;
};

namespace CameraECSAdapter
{
	Camera createFromComponents(const TransformComponent *tc, const CameraComponent *cc) noexcept;
	void updateComponents(const Camera &camera, ECS *ecs, EntityID entity) noexcept;
}