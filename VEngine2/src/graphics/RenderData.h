#pragma once
#include <stdint.h>
#include "Handles.h"
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>

struct SubMeshInstanceData
{
	SubMeshHandle m_subMeshHandle;
	uint32_t m_transformIndex;
	uint32_t m_skinningMatricesOffset;
	MaterialHandle m_materialHandle;
};

struct RenderList
{
	eastl::vector<SubMeshInstanceData> m_opaque;
	eastl::vector<SubMeshInstanceData> m_opaqueAlphaTested;
	eastl::vector<SubMeshInstanceData> m_opaqueSkinned;
	eastl::vector<SubMeshInstanceData> m_opaqueSkinnedAlphaTested;

	void clear() noexcept
	{
		m_opaque.clear();
		m_opaqueAlphaTested.clear();
		m_opaqueSkinned.clear();
		m_opaqueSkinnedAlphaTested.clear();
	}
};