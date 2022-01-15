#include "PhysicsComponent.h"
#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/gui_helpers.h"
#include <PxPhysicsAPI.h>

PhysicsComponent::PhysicsComponent(const PhysicsComponent &other) noexcept
	:m_mobility(other.m_mobility),
	m_physicsShapeType(other.m_physicsShapeType),
	m_sphereRadius(other.m_sphereRadius),
	m_planeNx(other.m_planeNx),
	m_planeNy(other.m_planeNy),
	m_planeNz(other.m_planeNz),
	m_planeDistance(other.m_planeDistance),
	m_convexMeshHandle(other.m_convexMeshHandle),
	m_triangleMeshHandle(other.m_triangleMeshHandle),
	m_linearDamping(other.m_linearDamping),
	m_angularDamping(other.m_angularDamping),
	m_density(other.m_density),
	m_initialVelocityX(other.m_initialVelocityX),
	m_initialVelocityY(other.m_initialVelocityY),
	m_initialVelocityZ(other.m_initialVelocityZ),
	m_materialHandle(other.m_materialHandle)
{
}

PhysicsComponent::PhysicsComponent(PhysicsComponent &&other) noexcept
	:m_mobility(other.m_mobility),
	m_physicsShapeType(other.m_physicsShapeType),
	m_sphereRadius(other.m_sphereRadius),
	m_planeNx(other.m_planeNx),
	m_planeNy(other.m_planeNy),
	m_planeNz(other.m_planeNz),
	m_planeDistance(other.m_planeDistance),
	m_convexMeshHandle(other.m_convexMeshHandle),
	m_triangleMeshHandle(other.m_triangleMeshHandle),
	m_linearDamping(other.m_linearDamping),
	m_angularDamping(other.m_angularDamping),
	m_density(other.m_density),
	m_initialVelocityX(other.m_initialVelocityX),
	m_initialVelocityY(other.m_initialVelocityY),
	m_initialVelocityZ(other.m_initialVelocityZ),
	m_materialHandle(other.m_materialHandle),
	m_internalPhysicsActorHandle(other.m_internalPhysicsActorHandle)
{
	other.m_internalPhysicsActorHandle = nullptr;
}

PhysicsComponent &PhysicsComponent::operator=(const PhysicsComponent &other) noexcept
{
	if (this != &other)
	{
		m_mobility = other.m_mobility;
		m_physicsShapeType = other.m_physicsShapeType;
		m_sphereRadius = other.m_sphereRadius;
		m_planeNx = other.m_planeNx;
		m_planeNy = other.m_planeNy;
		m_planeNz = other.m_planeNz;
		m_planeDistance = other.m_planeDistance;
		m_convexMeshHandle = other.m_convexMeshHandle;
		m_triangleMeshHandle = other.m_triangleMeshHandle;
		m_linearDamping = other.m_linearDamping;
		m_angularDamping = other.m_angularDamping;
		m_density = other.m_density;
		m_initialVelocityX = other.m_initialVelocityX;
		m_initialVelocityY = other.m_initialVelocityY;
		m_initialVelocityZ = other.m_initialVelocityZ;
		m_materialHandle = other.m_materialHandle;

		if (m_internalPhysicsActorHandle)
		{
			((physx::PxRigidDynamic *)m_internalPhysicsActorHandle)->release();
			m_internalPhysicsActorHandle = nullptr;
		}
	}
	return *this;
}

PhysicsComponent &PhysicsComponent::operator=(PhysicsComponent &&other) noexcept
{
	if (this != &other)
	{
		m_mobility = other.m_mobility;
		m_physicsShapeType = other.m_physicsShapeType;
		m_sphereRadius = other.m_sphereRadius;
		m_planeNx = other.m_planeNx;
		m_planeNy = other.m_planeNy;
		m_planeNz = other.m_planeNz;
		m_planeDistance = other.m_planeDistance;
		m_convexMeshHandle = other.m_convexMeshHandle;
		m_triangleMeshHandle = other.m_triangleMeshHandle;
		m_linearDamping = other.m_linearDamping;
		m_angularDamping = other.m_angularDamping;
		m_density = other.m_density;
		m_initialVelocityX = other.m_initialVelocityX;
		m_initialVelocityY = other.m_initialVelocityY;
		m_initialVelocityZ = other.m_initialVelocityZ;
		m_materialHandle = other.m_materialHandle;

		if (m_internalPhysicsActorHandle)
		{
			((physx::PxRigidDynamic *)m_internalPhysicsActorHandle)->release();
			m_internalPhysicsActorHandle = nullptr;
		}

		m_internalPhysicsActorHandle = other.m_internalPhysicsActorHandle;
		other.m_internalPhysicsActorHandle = nullptr;
	}
	return *this;
}

PhysicsComponent::~PhysicsComponent() noexcept
{
	if (m_internalPhysicsActorHandle)
	{
		((physx::PxRigidDynamic *)m_internalPhysicsActorHandle)->release();
		m_internalPhysicsActorHandle = nullptr;
	}
}

void PhysicsComponent::onGUI(void *instance, Renderer *renderer, const TransformComponent *transformComponent) noexcept
{
	PhysicsComponent &c = *reinterpret_cast<PhysicsComponent *>(instance);

	int mobilityInt = static_cast<int>(c.m_mobility);
	const char *mobilityStrings[] = { "STATIC", "DYNAMIC", "KINEMATIC" };
	ImGui::Combo("Mobility", &mobilityInt, mobilityStrings, IM_ARRAYSIZE(mobilityStrings));
	c.m_mobility = static_cast<PhysicsMobility>(mobilityInt);

	int shapeTypeInt = static_cast<int>(c.m_physicsShapeType);
	const char *shapeTypeStrings[] = { "SPHERE", "PLANE", "CONVEX_MESH", "TRIANGLE_MESH" };
	ImGui::Combo("Shape Type", &shapeTypeInt, shapeTypeStrings, IM_ARRAYSIZE(shapeTypeStrings));
	c.m_physicsShapeType = static_cast<PhysicsShapeType>(shapeTypeInt);

	switch (c.m_physicsShapeType)
	{
	case PhysicsShapeType::SPHERE:
		ImGui::DragFloat("Sphere Radius", &c.m_sphereRadius, 1.0f, 0.0f, FLT_MAX);
		ImGuiHelpers::Tooltip("The radius of the physics shape sphere.");
		break;
	case PhysicsShapeType::PLANE:
	{
		float planeN[] = { c.m_planeNx, c.m_planeNy, c.m_planeNz };
		ImGui::DragFloat3("Plane Normal", planeN, 0.01f, -1.0f, 1.0f);
		ImGuiHelpers::Tooltip("The normal vector of the plane.");
		glm::vec3 N = glm::normalize(glm::vec3(planeN[0], planeN[1], planeN[2]));
		c.m_planeNx = N[0];
		c.m_planeNy = N[1];
		c.m_planeNz = N[2];
		ImGui::DragFloat("Plane Distance", &c.m_planeDistance, 0.1f);
		ImGuiHelpers::Tooltip("The distance of the plane from the origin along the normal.");
	}
	break;
	case PhysicsShapeType::CONVEX_MESH:
		break;
	case PhysicsShapeType::TRIANGLE_MESH:
		break;
	default:
		assert(false);
		break;
	}

	ImGui::DragFloat("Linear Dampening", &c.m_linearDamping, 0.01f, 0.0f, FLT_MAX);

	ImGui::DragFloat("Angular Dampening", &c.m_angularDamping, 0.01f, 0.0f, FLT_MAX);

	ImGui::DragFloat("Density", &c.m_density, 0.01f, 0.01f, FLT_MAX);
}

void PhysicsComponent::toLua(lua_State *L, void *instance) noexcept
{
}

void PhysicsComponent::fromLua(lua_State *L, void *instance) noexcept
{
}
