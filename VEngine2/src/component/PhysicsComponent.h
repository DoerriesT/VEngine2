#pragma once
#include "Handles.h"

enum class PhysicsMobility : uint32_t
{
	STATIC = 0,
	DYNAMIC = 1,
	KINEMATIC = 2
};

enum class PhysicsShapeType : uint32_t
{
	SPHERE,
	PLANE,
	CONVEX_MESH,
	TRIANGLE_MESH
};

struct PhysicsComponent
{
	PhysicsMobility m_mobility = PhysicsMobility::STATIC;
	PhysicsShapeType m_physicsShapeType = PhysicsShapeType::SPHERE;
	float m_sphereRadius = 1.0f;
	float m_planeNx = 0.0f;
	float m_planeNy = 1.0f;
	float m_planeNz = 0.0f;
	float m_planeDistance = 0.0f;
	PhysicsConvexMeshHandle m_convexMeshHandle = {};
	PhysicsTriangleMeshHandle m_triangleMeshHandle = {};
	float m_linearDamping = 0.05f;
	float m_angularDamping = 0.05f;
	float m_density = 10.0f;
	float m_initialVelocityX = 0.0f;
	float m_initialVelocityY = 0.0f;
	float m_initialVelocityZ = 0.0f;
	PhysicsMaterialHandle m_materialHandle = {};
	void *m_internalPhysicsActorHandle = nullptr;

	static void onGUI(void *instance) noexcept;
	static const char *getComponentName() noexcept { return "PhysicsComponent"; }
	static const char *getComponentDisplayName() noexcept { return "Physics Component"; }
	static const char *getComponentTooltip() noexcept { return nullptr; }
};