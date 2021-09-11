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

AnimationClip::AnimationClip(const AnimationClipCreateInfo &createInfo) noexcept
	:m_jointCount(createInfo.m_jointCount),
	m_duration(createInfo.m_duration),
	m_memory(createInfo.m_memory),
	m_perJointInfo(createInfo.m_perJointInfo),
	m_translationTimeKeys(createInfo.m_translationTimeKeys),
	m_rotationTimeKeys(createInfo.m_rotationTimeKeys),
	m_scaleTimeKeys(createInfo.m_scaleTimeKeys),
	m_translationData(createInfo.m_translationData),
	m_rotationData(createInfo.m_rotationData),
	m_scaleData(createInfo.m_scaleData)
{
}

AnimationClip::AnimationClip(AnimationClip &&other) noexcept
	:m_jointCount(other.m_jointCount),
	m_duration(other.m_duration),
	m_memory(other.m_memory),
	m_perJointInfo(other.m_perJointInfo),
	m_translationTimeKeys(other.m_translationTimeKeys),
	m_rotationTimeKeys(other.m_rotationTimeKeys),
	m_scaleTimeKeys(other.m_scaleTimeKeys),
	m_translationData(other.m_translationData),
	m_rotationData(other.m_rotationData),
	m_scaleData(other.m_scaleData)
{
	other.m_jointCount = 0;
	other.m_duration = 0.0f;
	other.m_memory = nullptr;
	other.m_perJointInfo = nullptr;
	other.m_translationTimeKeys = nullptr;
	other.m_rotationTimeKeys = nullptr;
	other.m_scaleTimeKeys = nullptr;
	other.m_translationData = nullptr;
	other.m_rotationData = nullptr;
	other.m_scaleData = nullptr;
}

AnimationClip &AnimationClip::operator=(AnimationClip &&other) noexcept
{
	if (&other != this)
	{
		if (m_memory)
		{
			delete[] m_memory;
		}

		m_jointCount = other.m_jointCount;
		m_duration = other.m_duration;
		m_memory = other.m_memory;
		m_perJointInfo = other.m_perJointInfo;
		m_translationTimeKeys = other.m_translationTimeKeys;
		m_rotationTimeKeys = other.m_rotationTimeKeys;
		m_scaleTimeKeys = other.m_scaleTimeKeys;
		m_translationData = other.m_translationData;
		m_rotationData = other.m_rotationData;
		m_scaleData = other.m_scaleData;

		other.m_jointCount = 0;
		other.m_duration = 0.0f;
		other.m_memory = nullptr;
		other.m_perJointInfo = nullptr;
		other.m_translationTimeKeys = nullptr;
		other.m_rotationTimeKeys = nullptr;
		other.m_scaleTimeKeys = nullptr;
		other.m_translationData = nullptr;
		other.m_rotationData = nullptr;
		other.m_scaleData = nullptr;
	}
	return *this;
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

JointPose AnimationClip::getJointPose(size_t jointIdx, float time, bool loop, bool extractRootMotion) const noexcept
{
	assert(jointIdx < m_jointCount);

	JointPose jointPose{};
	jointPose.m_rot[3] = 1.0f;
	jointPose.m_scale = 1.0f;

	// translation
	{
		uint32_t frameCount = m_perJointInfo[jointIdx].m_translationFrameCount;
		size_t arrayOffset = m_perJointInfo[jointIdx].m_translationArrayOffset;

		if (frameCount)
		{
			const float *timeKeys = m_translationTimeKeys + arrayOffset;
			const float *data = m_translationData + (arrayOffset * 3);

			float adjustedTime = time;
			bool adjustedLoop = loop;

			if (jointIdx == 0 && extractRootMotion)
			{
				adjustedTime = 0.0f;
				adjustedLoop = false;
			}
			sampleData(3, adjustedTime, frameCount, adjustedLoop, timeKeys, data, jointPose.m_trans);

		}
	}

	// rotation
	{
		uint32_t frameCount = m_perJointInfo[jointIdx].m_rotationFrameCount;
		size_t arrayOffset = m_perJointInfo[jointIdx].m_rotationArrayOffset;

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
		uint32_t frameCount = m_perJointInfo[jointIdx].m_scaleFrameCount;
		size_t arrayOffset = m_perJointInfo[jointIdx].m_scaleArrayOffset;

		if (frameCount)
		{
			const float *timeKeys = m_scaleTimeKeys + arrayOffset;
			const float *data = m_scaleData + arrayOffset;

			sampleData(1, time, frameCount, loop, timeKeys, data, &jointPose.m_scale);
		}
	}

	return jointPose;
}
