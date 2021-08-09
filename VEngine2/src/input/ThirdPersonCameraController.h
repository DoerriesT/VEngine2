#pragma once

class Camera;
class ECS;
struct CharacterMovementComponent;
struct TransformComponent;

class ThirdPersonCameraController
{
public:
	explicit ThirdPersonCameraController(ECS *ecs) noexcept;
	void update(float timeDelta, Camera &camera, TransformComponent *characterTc, CharacterMovementComponent *movc);

private:
	ECS *m_ecs;
	float m_rotateHistory[2] = {};
	float m_zoomHistory = 0.0f;
	float m_cameraPitch = 0.5f;
	float m_cameraYaw = 0.0f;
	float m_cameraDistance = 10.0f;
};