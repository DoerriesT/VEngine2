#include "AnimationClip.h"
#include <assert.h>
#include <string.h>

static size_t findTimeKeyIndex(size_t count, const float *timeKeys, float time, float &alpha) noexcept
{
	alpha = 0.0f;
	size_t timeKeyIdx0 = 0;

	for (size_t i = 0; i < count; ++i)
	{
		if ((i + 1) == count || time < timeKeys[i + 1])
		{
			timeKeyIdx0 = i;
			break;
		}
	}

	if ((timeKeyIdx0 + 1) < count && count > 1)
	{
		const size_t timeKeyIdx1 = timeKeyIdx0 + 1;
		const float key0 = timeKeys[timeKeyIdx0];
		const float key1 = timeKeys[timeKeyIdx1];
		const float diff = key1 - key0;
		alpha = diff > 1e-5f ? (time - key0) / diff : 0.0f;
	}

	return timeKeyIdx0;
}

static void sampleData(uint32_t componentCount, float time, uint32_t frameCount, const float *timeKeys, const float *data, float *result) noexcept
{
	float alpha = 0.0f;
	const size_t timeKeyIdx0 = findTimeKeyIndex(frameCount, timeKeys, time, alpha);

	// time is smaller than first sample time key: clamp to first sample
	if (timeKeyIdx0 == 0 && timeKeys[0] >= time || frameCount == 1)
	{
		for (size_t i = 0; i < componentCount; ++i)
		{
			result[i] = data[i];
		}
	}
	// time is greater than last sample time key: clamp to last sample
	else if (timeKeyIdx0 == (frameCount - 1))
	{
		for (size_t i = 0; i < componentCount; ++i)
		{
			result[i] = data[(frameCount - 1) * componentCount + i];
		}
	}
	// lerp between adjacent samples
	else
	{
		auto lerp = [](auto x, auto y, auto alpha)
		{
			return x * (1.0f - alpha) + y * alpha;
		};

		for (size_t i = 0; i < componentCount; ++i)
		{
			result[i] = lerp(data[timeKeyIdx0 * componentCount + i], data[(timeKeyIdx0 + 1) * componentCount + i], alpha);
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

JointPose AnimationClip::getJointPose(size_t jointIdx, float time) const noexcept
{
	assert(jointIdx < m_jointCount);

	JointPose jointPose{};
	
	// translation
	{
		uint32_t frameCount = m_perJointInfo[jointIdx * 6 + 0];
		size_t arrayOffset = m_perJointInfo[jointIdx * 6 + 3];

		if (frameCount)
		{
			const float *timeKeys = m_translationTimeKeys + arrayOffset;
			const float *data = m_translationData + (arrayOffset * 3);

			sampleData(3, time, frameCount, timeKeys, data, jointPose.m_trans);
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

			sampleData(4, time, frameCount, timeKeys, data, jointPose.m_rot);
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

			sampleData(1, time, frameCount, timeKeys, data, &jointPose.m_scale);
		}
	}

	return jointPose;
}
