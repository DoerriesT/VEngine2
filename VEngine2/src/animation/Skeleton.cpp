#include "Skeleton.h"
#include "utility/Utility.h"

Skeleton::Skeleton(const SkeletonCreateInfo &createInfo) noexcept
	:m_jointCount(createInfo.m_jointCount),
	m_memory(createInfo.m_memory),
	m_invBindPoseMatrices(createInfo.m_invBindPoseMatrices),
	m_parentIndices(createInfo.m_parentIndices),
	m_jointNames(createInfo.m_jointNames)
{
}

Skeleton::Skeleton(Skeleton &&other) noexcept
	:m_jointCount(other.m_jointCount),
	m_memory(other.m_memory),
	m_invBindPoseMatrices(other.m_invBindPoseMatrices),
	m_parentIndices(other.m_parentIndices),
	m_jointNames(other.m_jointNames)
{
	other.m_jointCount = 0;
	other.m_memory = nullptr;
	other.m_invBindPoseMatrices = nullptr;
	other.m_parentIndices = nullptr;
	other.m_jointNames = nullptr;
}

Skeleton &Skeleton::operator=(Skeleton &&other) noexcept
{
	if (&other != this)
	{
		if (m_memory)
		{
			delete[] m_memory;
		}

		m_jointCount = other.m_jointCount;
		m_memory = other.m_memory;
		m_invBindPoseMatrices = other.m_invBindPoseMatrices;
		m_parentIndices = other.m_parentIndices;
		m_jointNames = other.m_jointNames;

		other.m_jointCount = 0;
		other.m_memory = nullptr;
		other.m_invBindPoseMatrices = nullptr;
		other.m_parentIndices = nullptr;
		other.m_jointNames = nullptr;
	}
	return *this;
}

Skeleton::~Skeleton()
{
	delete[] m_memory;
	m_memory = nullptr;
}

uint32_t Skeleton::getJointCount() const noexcept
{
	return m_jointCount;
}

const glm::mat4 *Skeleton::getInvBindPoseMatrices() const noexcept
{
	return m_invBindPoseMatrices;
}

const uint32_t *Skeleton::getParentIndices() const noexcept
{
	return m_parentIndices;
}

const char *Skeleton::getJointName(size_t jointIndex) const noexcept
{
	return jointIndex < m_jointCount ? m_jointNames[jointIndex].m_string : nullptr;
}
