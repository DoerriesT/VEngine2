#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

struct TransformComponent;
struct CameraComponent;

class Camera
{
public:
	explicit Camera(TransformComponent &transformComponent, CameraComponent &cameraComponent) noexcept;
	void setRotation(const glm::quat &rotation) noexcept;
	void setPosition(const glm::vec3 &position) noexcept;
	void setAspectRatio(float aspectRatio) noexcept;
	void setFovy(float fovy) noexcept;
	void rotate(const glm::vec3 &pitchYawRollOffset) noexcept;
	void translate(const glm::vec3 &translationOffset) noexcept;
	void lookAt(const glm::vec3 &targetPosition);
	glm::mat4 getViewMatrix() const noexcept;
	glm::mat4 getProjectionMatrix() const noexcept;
	glm::vec3 getPosition() const noexcept;
	glm::quat getRotation() const noexcept;
	glm::vec3 getForwardDirection() const noexcept;
	glm::vec3 getUpDirection() const noexcept;
	glm::vec3 getRightDirection() const noexcept;
	float getAspectRatio() const noexcept;
	float getFovy() const noexcept;

private:
	TransformComponent &m_transformComponent;
	CameraComponent &m_cameraComponent;

	void updateViewMatrix() noexcept;
	void updateProjectionMatrix() noexcept;
};