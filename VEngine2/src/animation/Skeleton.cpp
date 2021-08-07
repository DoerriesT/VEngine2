#include "Skeleton.h"
#include "utility/Utility.h"

Skeleton::Skeleton(size_t fileSize, const char *fileData) noexcept
{
	static_assert(sizeof(glm::mat4) == (sizeof(float) * 16));

	size_t curFileOffset = 0;

	m_jointCount = *reinterpret_cast<const uint32_t *>(fileData + curFileOffset);
	curFileOffset += 4;

	size_t parentIndicesDstOffset = 0;
	size_t namesDstOffset = 0;
	size_t rawCopySize = 0;

	size_t memorySize = 0;

	// matrices
	memorySize += m_jointCount * sizeof(glm::mat4);

	// parent indices
	memorySize = util::alignPow2Up(memorySize, alignof(uint32_t));
	parentIndicesDstOffset = memorySize;
	memorySize += m_jointCount * sizeof(uint32_t);

	// we cannot copy the strings from the file directly, so we only copy up to this point
	// and handle the strings manually
	rawCopySize = memorySize;

	// names
	memorySize = util::alignPow2Up(memorySize, alignof(StringID));
	namesDstOffset = memorySize;
	memorySize += m_jointCount * sizeof(StringID);

	// allocate memory
	m_memory = new char[memorySize];
	assert(m_memory);

	assert((curFileOffset + rawCopySize) < fileSize);

	// copy data to our allocation
	memcpy(m_memory, fileData + curFileOffset, rawCopySize);

	// set up pointers
	m_invBindPoseMatrices = reinterpret_cast<const glm::mat4 *>(m_memory);
	m_parentIndices = reinterpret_cast<const uint32_t *>(m_memory + parentIndicesDstOffset);
	m_jointNames = reinterpret_cast<const StringID *>(m_memory + namesDstOffset);


	// handle strings
	curFileOffset += rawCopySize;
	for (size_t i = 0; i < m_jointCount; ++i)
	{
		assert(curFileOffset < fileSize);

		size_t strLen = 0;
		while (fileData[curFileOffset + strLen] != '\0')
		{
			assert(curFileOffset + strLen < fileSize);
			++strLen;
		}

		assert(strLen);

		new (m_memory + namesDstOffset + i * sizeof(StringID)) StringID(fileData + curFileOffset);

		curFileOffset += strLen + 1; // dont forget the null terminator
	}
}

Skeleton::~Skeleton()
{
	// we placement newed the names, so call destructors now
	for (size_t i = 0; i < m_jointCount; ++i)
	{
		m_jointNames[i].~StringID();
	}

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
