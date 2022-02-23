#include "TransformHierarchy.h"
#include <assert.h>
#include "component/TransformComponent.h"
#include "ecs/ECS.h"
#include "Log.h"

namespace
{
	Transform getGlobalTransform(ECS *ecs, EntityID entity) noexcept
	{
		auto *tc = entity != k_nullEntity ? ecs->getComponent<TransformComponent>(entity) : nullptr;
		return tc ? tc->m_globalTransform : Transform{};
	}

	void updateTransformFromParent(ECS *ecs, EntityID entity, const Transform &parentTransform) noexcept
	{
		auto *tc = entity != k_nullEntity ? ecs->getComponent<TransformComponent>(entity) : nullptr;
		if (tc)
		{
			tc->m_globalTransform = Transform::updateFromParent(tc->m_transform, parentTransform);

			for (auto childEntity = tc->m_childEntity; childEntity != k_nullEntity; childEntity = TransformHierarchy::getNextSibling(ecs, childEntity))
			{
				updateTransformFromParent(ecs, childEntity, tc->m_globalTransform);
			}
		}
	}

	uint32_t updateMobilityFromParent(ECS *ecs, EntityID entity, Mobility mobility)
	{
		uint32_t changedMobilityCount = 0;

		auto *tc = entity != k_nullEntity ? ecs->getComponent<TransformComponent>(entity) : nullptr;
		if (tc)
		{
			changedMobilityCount += tc->m_mobility != mobility ? 1 : 0;
			tc->m_mobility = mobility;

			for (auto childEntity = tc->m_childEntity; childEntity != k_nullEntity; childEntity = TransformHierarchy::getNextSibling(ecs, childEntity))
			{
				changedMobilityCount += updateMobilityFromParent(ecs, childEntity, tc->m_mobility);
			}
		}

		return changedMobilityCount;
	}

	uint32_t propagateMobility(ECS *ecs, EntityID entity, TransformComponent *transformComponent = nullptr, TransformComponent *parentTransformComponent = nullptr)
	{
		assert(!transformComponent || transformComponent == ecs->getComponent<TransformComponent>(entity));
		auto *tc = transformComponent ? transformComponent : ecs->getComponent<TransformComponent>(entity);
		if (!tc)
		{
			return 0;
		}

		uint32_t changedMobilityCount = 0;

		// propagate mobility up to parents
		if (tc->m_mobility == Mobility::Static)
		{
			auto *ptc = parentTransformComponent;
			if (!ptc && tc->m_parentEntity != k_nullEntity)
			{
				ptc = ecs->getComponent<TransformComponent>(tc->m_parentEntity);
				assert(ptc);
			}

			if (ptc)
			{
				do
				{
					changedMobilityCount += ptc->m_mobility != tc->m_mobility ? 1 : 0;
					ptc->m_mobility = Mobility::Static;
					ptc = ptc->m_parentEntity != k_nullEntity ? ecs->getComponent<TransformComponent>(ptc->m_parentEntity) : nullptr;
				} while (ptc);
			}
		}
		// propagate mobility down to children
		else if (tc->m_mobility == Mobility::Dynamic)
		{
			for (auto childEntity = tc->m_childEntity; childEntity != k_nullEntity; childEntity = TransformHierarchy::getNextSibling(ecs, childEntity))
			{
				changedMobilityCount += updateMobilityFromParent(ecs, childEntity, Mobility::Dynamic);
			}
		}

		return changedMobilityCount;
	}

	bool findLoop(ECS *ecs, EntityID entity, EntityID proposedParent) noexcept
	{
		auto p = proposedParent;
		while (p != k_nullEntity)
		{
			if (p == entity)
			{
				return true;
			}
			auto *tc = ecs->getComponent<TransformComponent>(p);
			p = tc ? tc->m_parentEntity : k_nullEntity;
		}

		return false;
	}
}

bool TransformHierarchy::attach(ECS *ecs, EntityID childEntity, EntityID parentEntity, bool keepLocalTransform, TransformComponent *childTransformComponent) noexcept
{
	assert(!childTransformComponent || childTransformComponent == ecs->getComponent<TransformComponent>(childEntity));

	// graph must be acyclic
	if (findLoop(ecs, childEntity, parentEntity))
	{
		return false;
	}

	auto *tc = childTransformComponent ? childTransformComponent : ecs->getOrAddComponent<TransformComponent>(childEntity);

	// already attached -> detach
	if (tc->m_parentEntity == parentEntity)
	{
		parentEntity = k_nullEntity;
	}

	// detach from old parent
	detach(ecs, childEntity, false, tc);

	// attach to new parent
	if (parentEntity != k_nullEntity)
	{
		auto *ptc = ecs->getOrAddComponent<TransformComponent>(parentEntity);

		auto oldChildEntity = ptc->m_childEntity;

		// parent entity now points to the new child as head of child list
		ptc->m_childEntity = childEntity;

		// set parent and new siblings
		tc->m_parentEntity = parentEntity;
		tc->m_nextSiblingEntity = oldChildEntity;
		if (tc->m_nextSiblingEntity != k_nullEntity)
		{
			auto *stc = ecs->getComponent<TransformComponent>(tc->m_nextSiblingEntity);
			assert(stc && stc->m_prevSiblingEntity == k_nullEntity);
			stc->m_prevSiblingEntity = childEntity;
		}

		uint32_t changedMobilityCount = propagateMobility(ecs, childEntity, tc, ptc);
		if (changedMobilityCount != 0)
		{
			Log::info("Transform hierarchy modification caused %u entities to change their mobility.", (unsigned)changedMobilityCount);
		}
	}

	// make local transform relative to new parent
	{
		auto *ptc = parentEntity != k_nullEntity ? ecs->getComponent<TransformComponent>(parentEntity) : nullptr;
		assert(parentEntity == k_nullEntity || ptc);
		if (keepLocalTransform)
		{
			if (parentEntity != k_nullEntity)
			{
				tc->m_globalTransform = Transform::updateFromParent(tc->m_transform, ptc->m_globalTransform);

				// update children
				for (auto childEntity = tc->m_childEntity; childEntity != k_nullEntity; childEntity = getNextSibling(ecs, childEntity))
				{
					updateTransformFromParent(ecs, childEntity, tc->m_globalTransform);
				}
			}
		}
		else
		{
			tc->m_transform = parentEntity != k_nullEntity ? Transform::makeRelative(tc->m_globalTransform, ptc->m_globalTransform) : tc->m_globalTransform;
			// no need to update children recursively, because the attaching operation left the global transform as it is
		}
	}

	return true;
}

void TransformHierarchy::detach(ECS *ecs, EntityID entity, bool orphanChildren, TransformComponent *transformComponent) noexcept
{
	assert(!transformComponent || transformComponent == ecs->getComponent<TransformComponent>(entity));
	auto *tc = transformComponent ? transformComponent : ecs->getComponent<TransformComponent>(entity);
	if (!tc)
	{
		return;
	}

	if (tc->m_parentEntity != k_nullEntity)
	{
		auto *ptc = ecs->getComponent<TransformComponent>(tc->m_parentEntity);
		assert(ptc && ptc->m_childEntity != k_nullEntity);

		// fix child pointer of old parent
		if (ptc->m_childEntity == entity)
		{
			ptc->m_childEntity = tc->m_nextSiblingEntity;
		}

		// fix prev and next sibling pointers of siblings
		if (tc->m_prevSiblingEntity != k_nullEntity)
		{
			auto *stc = ecs->getComponent<TransformComponent>(tc->m_prevSiblingEntity);
			assert(stc && stc->m_nextSiblingEntity == entity);
			stc->m_nextSiblingEntity = tc->m_nextSiblingEntity;
		}
		if (tc->m_nextSiblingEntity != k_nullEntity)
		{
			auto *stc = ecs->getComponent<TransformComponent>(tc->m_nextSiblingEntity);
			assert(stc && stc->m_prevSiblingEntity == entity);
			stc->m_prevSiblingEntity = tc->m_prevSiblingEntity;
		}

		tc->m_parentEntity = k_nullEntity;
		tc->m_prevSiblingEntity = k_nullEntity;
		tc->m_nextSiblingEntity = k_nullEntity;
	}

	if (orphanChildren)
	{
		auto childEntity = tc->m_childEntity;
		while (childEntity != k_nullEntity)
		{
			auto *ctc = ecs->getComponent<TransformComponent>(childEntity);
			assert(ctc);

			auto nextChild = ctc->m_nextSiblingEntity;

			ctc->m_parentEntity = k_nullEntity;
			ctc->m_prevSiblingEntity = k_nullEntity;
			ctc->m_nextSiblingEntity = k_nullEntity;

			childEntity = nextChild;
		}

		tc->m_childEntity = k_nullEntity;
	}
}

void TransformHierarchy::setLocalTransform(ECS *ecs, EntityID entity, const Transform &transform, TransformComponent *transformComponent) noexcept
{
	assert(!transformComponent || transformComponent == ecs->getComponent<TransformComponent>(entity));
	auto *tc = transformComponent ? transformComponent : ecs->getComponent<TransformComponent>(entity);
	if (!tc)
	{
		return;
	}

	tc->m_transform = transform;
	tc->m_globalTransform = Transform::updateFromParent(transform, getGlobalTransform(ecs, tc->m_parentEntity));

	for (auto childEntity = tc->m_childEntity; childEntity != k_nullEntity; childEntity = getNextSibling(ecs, childEntity))
	{
		updateTransformFromParent(ecs, childEntity, tc->m_globalTransform);
	}
}

void TransformHierarchy::setGlobalTransform(ECS *ecs, EntityID entity, const Transform &transform, TransformComponent *transformComponent) noexcept
{
	assert(!transformComponent || transformComponent == ecs->getComponent<TransformComponent>(entity));
	auto *tc = transformComponent ? transformComponent : ecs->getComponent<TransformComponent>(entity);
	if (!tc)
	{
		return;
	}

	tc->m_globalTransform = transform;
	tc->m_transform = Transform::makeRelative(transform, getGlobalTransform(ecs, tc->m_parentEntity));

	for (auto childEntity = tc->m_childEntity; childEntity != k_nullEntity; childEntity = getNextSibling(ecs, childEntity))
	{
		updateTransformFromParent(ecs, childEntity, tc->m_globalTransform);
	}
}

uint32_t TransformHierarchy::setMobility(ECS *ecs, EntityID entity, Mobility mobility, TransformComponent *transformComponent) noexcept
{
	assert(!transformComponent || transformComponent == ecs->getComponent<TransformComponent>(entity));
	auto *tc = transformComponent ? transformComponent : ecs->getComponent<TransformComponent>(entity);
	if (!tc)
	{
		return 0;
	}

	if (tc->m_mobility == mobility)
	{
		return 0;
	}
	tc->m_mobility = mobility;

	uint32_t changedMobilityCount = propagateMobility(ecs, entity, tc);
	if (changedMobilityCount != 0)
	{
		Log::info("Change of entity mobility caused %u other entities to change their mobility.", (unsigned)changedMobilityCount);
	}

	return changedMobilityCount;
}

EntityID TransformHierarchy::getNextSibling(ECS *ecs, EntityID entity) noexcept
{
	auto *tc = entity != k_nullEntity ? ecs->getComponent<TransformComponent>(entity) : nullptr;
	assert(entity == k_nullEntity || tc);
	return tc ? tc->m_nextSiblingEntity : k_nullEntity;
}
