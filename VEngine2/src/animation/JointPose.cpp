#include "JointPose.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "utility/Utility.h"

JointPose JointPose::lerp(const JointPose &x, const JointPose &y, float alpha) noexcept
{
	JointPose result{};

	result.m_trans[0] = x.m_trans[0] * (1.0f - alpha) + y.m_trans[0] * alpha;
	result.m_trans[1] = x.m_trans[1] * (1.0f - alpha) + y.m_trans[1] * alpha;
	result.m_trans[2] = x.m_trans[2] * (1.0f - alpha) + y.m_trans[2] * alpha;

	auto q0 = glm::make_quat(x.m_rot);
	auto q1 = glm::make_quat(y.m_rot);
	auto resultQ = glm::normalize(glm::slerp(q0, q1, alpha));
	memcpy(result.m_rot, &resultQ[0], sizeof(result.m_rot));

	result.m_scale = x.m_scale * (1.0f - alpha) + y.m_scale * alpha;

	return result;
}

JointPose JointPose::lerp1DArray(size_t count, const JointPose *poses, const float *poseKeys, float lookupKey) noexcept
{
	size_t index0;
	size_t index1;
	float alpha;
	util::findPieceWiseLinearCurveIndicesAndAlpha(count, poseKeys, lookupKey, false, &index0, &index1, &alpha);

	if (index0 == index1)
	{
		return poses[index0];
	}
	else
	{
		return lerp(poses[index0], poses[index1], alpha);
	}
}

JointPose JointPose::lerp2D(const JointPose &tl, const JointPose &tr, const JointPose &bl, const JointPose &br, float horizontalAlpha, float verticalAlpha) noexcept
{
	return lerp(lerp(tl, tr, horizontalAlpha), lerp(bl, br, horizontalAlpha), verticalAlpha);
}
