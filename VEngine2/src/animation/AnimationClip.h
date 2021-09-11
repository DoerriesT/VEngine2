#pragma once
#include <stdint.h>
#include "utility/DeletedCopyMove.h"
#include "JointPose.h"

struct AnimationClipJointInfo
{
	uint32_t m_translationFrameCount;
	uint32_t m_rotationFrameCount;
	uint32_t m_scaleFrameCount;
	uint32_t m_translationArrayOffset;
	uint32_t m_rotationArrayOffset;
	uint32_t m_scaleArrayOffset;
};

static_assert(sizeof(AnimationClipJointInfo) == (6 * sizeof(uint32_t)));
static_assert(alignof(AnimationClipJointInfo) == alignof(uint32_t));

struct AnimationClipCreateInfo
{
	uint32_t m_jointCount;
	float m_duration;
	const char *m_memory;
	const AnimationClipJointInfo *m_perJointInfo;
	const float *m_translationTimeKeys = nullptr;
	const float *m_rotationTimeKeys = nullptr;
	const float *m_scaleTimeKeys = nullptr;
	const float *m_translationData = nullptr; // 3 floats per entry
	const float *m_rotationData = nullptr; // 4 floats (quaternion xyzw) per entry
	const float *m_scaleData = nullptr; // 1 float per entry (uniform scale)
};

class AnimationClip
{
public:
	explicit AnimationClip() noexcept = default;
	explicit AnimationClip(const AnimationClipCreateInfo &createInfo) noexcept;
	AnimationClip(AnimationClip &&other) noexcept;
	AnimationClip &operator=(AnimationClip &&other) noexcept;
	DELETED_COPY(AnimationClip);
	~AnimationClip();
	float getDuration() const noexcept;
	uint32_t getJointCount() const noexcept;
	JointPose getJointPose(size_t jointIdx, float time, bool loop, bool extractRootMotion) const noexcept;

private:
	uint32_t m_jointCount = 0;
	float m_duration = 0.0f;
	const char *m_memory = nullptr;
	const AnimationClipJointInfo *m_perJointInfo = nullptr;
	const float *m_translationTimeKeys = nullptr;
	const float *m_rotationTimeKeys = nullptr;
	const float *m_scaleTimeKeys = nullptr;
	const float *m_translationData = nullptr; // 3 floats per entry
	const float *m_rotationData = nullptr; // 4 floats (quaternion xyzw) per entry
	const float *m_scaleData = nullptr; // 1 float per entry (uniform scale)
};