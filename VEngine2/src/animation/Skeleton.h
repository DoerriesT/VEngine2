#pragma once
#include <stdint.h>
#include "utility/StringID.h"
#include "utility/DeletedCopyMove.h"
#include <glm/mat4x4.hpp>

class Skeleton
{
public:
	explicit Skeleton(size_t fileSize, const char *fileData) noexcept;
	DELETED_COPY_MOVE(Skeleton);
	~Skeleton();
	uint32_t getJointCount() const noexcept;
	const glm::mat4 *getInvBindPoseMatrices() const noexcept;
	const uint32_t *getParentIndices() const noexcept;
	const char *getJointName(size_t jointIndex) const noexcept;

private:
	uint32_t m_jointCount = 0;
	char *m_memory = nullptr;
	const glm::mat4 *m_invBindPoseMatrices = nullptr;
	const uint32_t *m_parentIndices = nullptr;
	const StringID *m_jointNames = nullptr;
};