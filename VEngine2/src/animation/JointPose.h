#pragma once

struct JointPose
{
	float m_rot[4];
	float m_trans[3];
	float m_scale;

	/// <summary>
	/// Performs linear interpolation between the two input JointPoses and returns the resulting JointPose.
	/// </summary>
	/// <param name="x">The first JointPose to interpolate.</param>
	/// <param name="y">The second JointPose to interpolate.</param>
	/// <param name="alpha">The alpha value to interpolate between x and y.
	/// A value of 0.0f returns x, a value of 1.0f returns y and values between 0.0f and 1.0f
	/// return the corresponding result of linear interplation.</param>
	/// <returns>The interpolated JointPose.</returns>
	static JointPose lerp(const JointPose &x, const JointPose &y, float alpha) noexcept;

	/// <summary>
	/// Takes an array of JointPoses, an array of corresponding pose keys (sorted in ascending order)
	/// and a lookup key value. It then finds two JointPoses such that the pose key of the first JointPose is
	/// closest to but less-equal to the lookup key and the second is closest to but greater than the lookup key.
	/// Both JointPoses are then interpolated with an alpha value proportional to how close the lookup key is to either
	/// pose key. 
	/// </summary>
	/// <param name="count">The number JointPoses and corresponding keys.</param>
	/// <param name="poses">A pointer to an array of JointPoses.</param>
	/// <param name="poseKeys">A pointer to an array of pose keys corresponding to each entry of poses. Must be sorted in ascending order.</param>
	/// <param name="lookupKey">The lookup key.</param>
	/// <returns></returns>
	static JointPose lerp1DArray(size_t count, const JointPose *poses, const float *poseKeys, float lookupKey) noexcept;

	/// <summary>
	/// Performs linear interpolation between four input JointPoses and returns the resulting JointPose.
	/// </summary>
	/// <param name="tl">"Top left" input JointPose.</param>
	/// <param name="tr">"Top right" input JointPose.</param>
	/// <param name="bl">"Bottom left" input JointPose.</param>
	/// <param name="br">"Bottom right" input JointPose.</param>
	/// <param name="horizontalAlpha">The alpha value for interpolating between "left" and "right" inputs.</param>
	/// <param name="verticalAlpha">The alpha value for interpolating between "top" and "bottom" inputs.</param>
	/// <returns>The intepolated JointPose.</returns>
	static JointPose lerp2D(const JointPose &tl, const JointPose &tr, const JointPose &bl, const JointPose &br, float horizontalAlpha, float verticalAlpha) noexcept;
};