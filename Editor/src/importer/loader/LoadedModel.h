#pragma once
#include <EASTL/vector.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <EASTL/string.h>

#ifdef OPAQUE
#undef OPAQUE
#endif

struct LoadedSkeleton
{
	struct Joint
	{
		glm::mat4 m_invBindPose; // inverse bind pose transform (transforms from model space to joint space)
		eastl::string m_name; // human-readable joint name
		uint32_t m_parentIdx; // parent index or -1 if root
	};

	eastl::vector<Joint> m_joints;
};

struct LoadedAnimationClip
{
	struct TranslationChannel
	{
		eastl::vector<float> m_timeKeys;
		eastl::vector<glm::vec3> m_translations;
	};

	struct RotationChannel
	{
		eastl::vector<float> m_timeKeys;
		eastl::vector<glm::quat> m_rotations;
	};

	struct ScaleChannel
	{
		eastl::vector<float> m_timeKeys;
		eastl::vector<float> m_scales;
	};

	struct JointAnimationClip
	{
		TranslationChannel m_translationChannel;
		RotationChannel m_rotationChannel;
		ScaleChannel m_scaleChannel;
	};

	eastl::string m_name;
	size_t m_skeletonIndex;
	float m_duration;
	eastl::vector<JointAnimationClip> m_jointAnimations;
};

struct LoadedMaterial
{
	enum class Alpha
	{
		OPAQUE, MASKED, BLENDED
	};

	eastl::string m_name;
	Alpha m_alpha;
	glm::vec3 m_albedoFactor;
	float m_metalnessFactor;
	float m_roughnessFactor;
	glm::vec3 m_emissiveFactor;
	float m_opacity;
	eastl::string m_albedoTexture;
	eastl::string m_normalTexture;
	eastl::string m_metalnessTexture;
	eastl::string m_roughnessTexture;
	eastl::string m_occlusionTexture;
	eastl::string m_emissiveTexture;
	eastl::string m_displacementTexture;
};

struct LoadedModel
{
	struct Mesh
	{
		eastl::string m_name;
		size_t m_materialIndex;
		glm::vec3 m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
		glm::vec3 m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
		eastl::vector<glm::vec3> m_positions;
		eastl::vector<glm::vec3> m_normals;
		eastl::vector<glm::vec4> m_tangents;
		eastl::vector<glm::vec2> m_texCoords;
		eastl::vector<glm::vec4> m_weights;
		eastl::vector<glm::uvec4> m_joints;
	};

	glm::vec3 m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
	eastl::vector<Mesh> m_meshes;
	eastl::vector<LoadedMaterial> m_materials;
	eastl::vector<LoadedSkeleton> m_skeletons;
	eastl::vector<LoadedAnimationClip> m_animationClips;
};