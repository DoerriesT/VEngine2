#pragma once

class ECS;
class Camera;
struct CharacterMovementComponent;

class FPSCameraController
{
public:
	explicit FPSCameraController(ECS *ecs) noexcept;
	void update(float timeDelta, Camera &camera, CharacterMovementComponent *movc) noexcept;

private:
	ECS *m_ecs = nullptr;
	float m_rotateHistory[2] = {};
};