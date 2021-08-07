#include "AnimationClip.h"
#include <assert.h>
#include <string.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "utility/Utility.h"


static void sampleData(uint32_t componentCount, float time, uint32_t frameCount, bool loop, const float *timeKeys, const float *data, float *result) noexcept
{
	float alpha = 0.0f;
	size_t index0 = 0;
	size_t index1 = 0;
	util::findPieceWiseLinearCurveIndicesAndAlpha(frameCount, timeKeys, time, loop, &index0, &index1, &alpha);

	auto lerp = [](auto x, auto y, auto alpha)
	{
		return x * (1.0f - alpha) + y * alpha;
	};

	if (index0 == index1)
	{
		for (size_t i = 0; i < componentCount; ++i)
		{
			result[i] = data[index0 * componentCount + i];
		}
	}
	else
	{
		for (size_t i = 0; i < componentCount; ++i)
		{
			result[i] = lerp(data[index0 * componentCount + i], data[index1 * componentCount + i], alpha);
		}
	}

	
}

AnimationClip::AnimationClip(size_t fileSize, const char *fileData) noexcept
{
	// at least jointCount and duration
	assert(fileSize >= 8);

	size_t curFileOffset = 0;

	m_jointCount = *reinterpret_cast<const uint32_t *>(fileData + curFileOffset);
	curFileOffset += 4;

	m_duration = *reinterpret_cast<const float *>(fileData + curFileOffset);
	curFileOffset += 4;

	const uint32_t *jointInfo = reinterpret_cast<const uint32_t *>(fileData + curFileOffset);

	const size_t lastEntryIdx = m_jointCount - 1;


	assert((curFileOffset + m_jointCount * 6 * sizeof(uint32_t)) < fileSize);

	const uint32_t lastEntryTranslationFrameCount = jointInfo[lastEntryIdx * 6 + 0];
	const uint32_t lastEntryRotationFrameCount = jointInfo[lastEntryIdx * 6 + 1];
	const uint32_t lastEntryScaleFrameCount = jointInfo[lastEntryIdx * 6 + 2];
	const uint32_t lastEntryTranslationOffset = jointInfo[lastEntryIdx * 6 + 3];
	const uint32_t lastEntryRotationOffset = jointInfo[lastEntryIdx * 6 + 4];
	const uint32_t lastEntryScaleOffset = jointInfo[lastEntryIdx * 6 + 5];

	const uint32_t translationCount = lastEntryTranslationOffset + lastEntryTranslationFrameCount;
	const uint32_t rotationCount = lastEntryRotationOffset + lastEntryRotationFrameCount;
	const uint32_t scaleCount = lastEntryScaleOffset + lastEntryScaleFrameCount;

	size_t memorySize = 0;
	memorySize += m_jointCount * 6 * sizeof(uint32_t); // per joint info
	memorySize += (translationCount + rotationCount + scaleCount) * sizeof(float); // time keys
	memorySize += translationCount * sizeof(float) * 3; // translation data
	memorySize += rotationCount * sizeof(float) * 4; // rotation data
	memorySize += scaleCount * sizeof(float) * 1; // scale data

	assert((curFileOffset + memorySize) <= fileSize);

	// allocate memory
	m_memory = new char[memorySize];
	assert(m_memory);

	// copy data to our allocation
	memcpy(m_memory, fileData + curFileOffset, memorySize);

	// set up pointers
	size_t curMemOffset = 0;

	m_perJointInfo = reinterpret_cast<const uint32_t *>(m_memory + curMemOffset);
	curMemOffset += m_jointCount * 6 * sizeof(uint32_t);

	m_translationTimeKeys = reinterpret_cast<const float *>(m_memory + curMemOffset);
	curMemOffset += translationCount * sizeof(float);

	m_rotationTimeKeys = reinterpret_cast<const float *>(m_memory + curMemOffset);
	curMemOffset += rotationCount * sizeof(float);

	m_scaleTimeKeys = reinterpret_cast<const float *>(m_memory + curMemOffset);
	curMemOffset += scaleCount * sizeof(float);

	m_translationData = reinterpret_cast<const float *>(m_memory + curMemOffset);
	curMemOffset += translationCount * sizeof(float) * 3;

	m_rotationData = reinterpret_cast<const float *>(m_memory + curMemOffset);
	curMemOffset += rotationCount * sizeof(float) * 4;

	m_scaleData = reinterpret_cast<const float *>(m_memory + curMemOffset);
	curMemOffset += scaleCount * sizeof(float) * 1;
}

AnimationClip::~AnimationClip()
{
	delete[] m_memory;
	m_memory = nullptr;
}

float AnimationClip::getDuration() const noexcept
{
	return m_duration;
}

uint32_t AnimationClip::getJointCount() const noexcept
{
	return m_jointCount;
}

JointPose AnimationClip::getJointPose(size_t jointIdx, float time, bool loop) const noexcept
{
	assert(jointIdx < m_jointCount);

	JointPose jointPose{};
	jointPose.m_rot[3] = 1.0f;
	jointPose.m_scale = 1.0f;

	// translation
	{
		uint32_t frameCount = m_perJointInfo[jointIdx * 6 + 0];
		size_t arrayOffset = m_perJointInfo[jointIdx * 6 + 3];

		if (frameCount)
		{
			const float *timeKeys = m_translationTimeKeys + arrayOffset;
			const float *data = m_translationData + (arrayOffset * 3);

			sampleData(3, time, frameCount, loop, timeKeys, data, jointPose.m_trans);
		}
	}

	// rotation
	{
		uint32_t frameCount = m_perJointInfo[jointIdx * 6 + 1];
		size_t arrayOffset = m_perJointInfo[jointIdx * 6 + 4];

		if (frameCount)
		{
			const float *timeKeys = m_rotationTimeKeys + arrayOffset;
			const float *data = m_rotationData + (arrayOffset * 4);

#if 0
			sampleData(4, time, frameCount, timeKeys, data, jointPose.m_rot);
#else
			float alpha = 0.0f;
			size_t index0 = 0;
			size_t index1 = 0;
			util::findPieceWiseLinearCurveIndicesAndAlpha(frameCount, timeKeys, time, loop, &index0, &index1, &alpha);

			glm::quat resultQ = glm::identity<glm::quat>();

			if (index0 == index1)
			{
				resultQ = glm::make_quat(&data[index0 * 4]);
			}
			else
			{
				glm::quat q0 = glm::make_quat(&data[index0 * 4]);
				glm::quat q1 = glm::make_quat(&data[index1 * 4]);
				resultQ = glm::normalize(glm::slerp(q0, q1, alpha));
			}

			memcpy(jointPose.m_rot, &resultQ[0], sizeof(jointPose.m_rot));
#endif
		}
	}

	// scale
	{
		uint32_t frameCount = m_perJointInfo[jointIdx * 6 + 2];
		size_t arrayOffset = m_perJointInfo[jointIdx * 6 + 5];

		if (frameCount)
		{
			const float *timeKeys = m_scaleTimeKeys + arrayOffset;
			const float *data = m_scaleData + arrayOffset;

			sampleData(1, time, frameCount, loop, timeKeys, data, &jointPose.m_scale);
		}
	}

	return jointPose;
}