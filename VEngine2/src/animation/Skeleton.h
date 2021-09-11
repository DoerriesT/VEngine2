#pragma once
#include <stdint.h>
#include "utility/StringID.h"
#include "utility/DeletedCopyMove.h"
#include <glm/mat4x4.hpp>

struct SkeletonCreateInfo
{
	uint32_t m_jointCount;
	const glm::mat4 *m_invBindPoseMatrices;
	const uint32_t *m_parentIndices;
	const StringID *m_jointNames;
	const char *m_memory;
};

class Skeleton
{
public:
	explicit Skeleton() noexcept = default;
	explicit Skeleton(const SkeletonCreateInfo &createInfo) noexcept;
	Skeleton(Skeleton &&other) noexcept;
	Skeleton &operator=(Skeleton &&other) noexcept;
	DELETED_COPY(Skeleton);
	~Skeleton();
	uint32_t getJointCount() const noexcept;
	const glm::mat4 *getInvBindPoseMatrices() const noexcept;
	const uint32_t *getParentIndices() const noexcept;
	const char *getJointName(size_t jointIndex) const noexcept;

private:
	uint32_t m_jointCount = 0;
	const char *m_memory = nullptr;
	const glm::mat4 *m_invBindPoseMatrices = nullptr;
	const uint32_t *m_parentIndices = nullptr;
	const StringID *m_jointNames = nullptr;
};