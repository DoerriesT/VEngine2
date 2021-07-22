#pragma once
#include <vector>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <string>

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
};