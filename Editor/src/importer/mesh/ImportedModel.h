#pragma once
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <string>

struct ImportedJoint
{
	glm::mat4 m_invBindPose; // inverse bind pose transform (transforms from model space to joint space)
	std::string m_name; // human-readable joint name
	uint32_t m_parentIdx; // parent index or -1 if root
};

struct ImportedSkeleton
{
	std::vector<ImportedJoint> m_joints;
};

struct ImportedAnimTranslationChannel
{
	std::vector<float> m_timeKeys;
	std::vector<glm::vec3> m_translations;
};

struct ImportedAnimRotationChannel
{
	std::vector<float> m_timeKeys;
	std::vector<glm::quat> m_rotations;
};

struct ImportedAnimScaleChannel
{
	std::vector<float> m_timeKeys;
	std::vector<float> m_scales;
};

struct ImportedJointAnimationClip
{
	ImportedAnimTranslationChannel m_translationChannel;
	ImportedAnimRotationChannel m_rotationChannel;
	ImportedAnimScaleChannel m_scaleChannel;
};

struct ImportedAnimationClip
{
	std::string m_name;
	size_t m_skeletonIndex;
	float m_duration;
	std::vector<ImportedJointAnimationClip> m_jointAnimations;
};

struct ImportedMaterial
{
	enum class Alpha
	{
		OPAQUE, MASKED, BLENDED
	};

	std::string m_name;
	Alpha m_alpha;
	glm::vec3 m_albedoFactor;
	float m_metalnessFactor;
	float m_roughnessFactor;
	glm::vec3 m_emissiveFactor;
	float m_opacity;
	std::string m_albedoTexture;
	std::string m_normalTexture;
	std::string m_metalnessTexture;
	std::string m_roughnessTexture;
	std::string m_occlusionTexture;
	std::string m_emissiveTexture;
	std::string m_displacementTexture;
};

struct ImportedMesh
{
	std::string m_name;
	size_t m_materialIndex;
	glm::vec3 m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
	std::vector<glm::vec3> m_positions;
	std::vector<glm::vec3> m_normals;
	std::vector<glm::vec4> m_tangents;
	std::vector<glm::vec2> m_texCoords;
	std::vector<glm::vec4> m_weights;
	std::vector<glm::uvec4> m_joints;
};

struct ImportedModel
{
	glm::vec3 m_aabbMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 m_aabbMax = glm::vec3(std::numeric_limits<float>::lowest());
	std::vector<ImportedMesh> m_meshes;
	std::vector<ImportedMaterial> m_materials;
	std::vector<ImportedSkeleton> m_skeletons;
	std::vector<ImportedAnimationClip> m_animationClips;
};