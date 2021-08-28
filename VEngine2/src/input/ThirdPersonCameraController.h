#pragma once

class Camera;
class ECS;
struct CharacterMovementComponent;
struct TransformComponent;
class Physics;

class ThirdPersonCameraController
{
public:
	explicit ThirdPersonCameraController(ECS *ecs) noexcept;
	void update(float timeDelta, Camera &camera, TransformComponent *characterTc, CharacterMovementComponent *movc, Physics *physics);

private:
	ECS *m_ecs;
	float m_rotateHistory[2] = {};
	float m_zoomHistory = 0.0f;
	float m_cameraPitch = 0.5f;
	float m_cameraYaw = 0.0f;
	float m_cameraTargetDistance = 10.0f;
	float m_prevCameraDistance = 10.0f;
};