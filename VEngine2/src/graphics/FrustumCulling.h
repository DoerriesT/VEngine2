#pragma once
#include "RenderData.h"

namespace FrustumCulling
{
	struct FrustumInfo
	{
		glm::vec4 m_planes[6];
	};

	FrustumInfo createFrustumInfo(const glm::mat4 &viewProjection) noexcept;
	bool cull(const FrustumInfo &frustum, const glm::vec4 &boundingSphere) noexcept;

	// inputIndices may be null
	size_t cull(size_t count, const uint32_t *inputIndices, const glm::vec4 *boundingSpheres, const glm::mat4 &viewProjection, uint32_t *resultIndices) noexcept;
}