#pragma once
#include <stdint.h>
#include "utility/DeletedCopyMove.h"
#include "JointPose.h"
#include <EASTL/vector.h>

class AnimationClip
{
public:
	explicit AnimationClip(size_t fileSize, const char *fileData) noexcept;
	DELETED_COPY_MOVE(AnimationClip);
	~AnimationClip();
	float getDuration() const noexcept;
	uint32_t getJointCount() const noexcept;
	JointPose getJointPose(size_t jointIdx, float time, bool loop, bool extractRootMotion) const noexcept;
	void getRootVelocity(float time, bool loop, float *result) const noexcept;

private:
	uint32_t m_jointCount = 0;
	float m_duration = 0.0f;
	char *m_memory = nullptr;

	// 6 uints per joint: 
	// 0: translation frame count
	// 1: rotation frame count
	// 2: scale frame count
	// 3: translation array offset
	// 4: rotation array offset
	// 5: scale array offset
	const uint32_t *m_perJointInfo = nullptr;
	const float *m_translationTimeKeys = nullptr;
	const float *m_rotationTimeKeys = nullptr;
	const float *m_scaleTimeKeys = nullptr;
	const float *m_translationData = nullptr; // 3 floats per entry
	const float *m_rotationData = nullptr; // 4 floats (quaternion xyzw) per entry
	const float *m_scaleData = nullptr; // 1 float per entry (uniform scale)
	eastl::vector<float> m_rootVelocityData;
};