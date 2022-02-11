#pragma once
#include "RenderData.h"

namespace FrustumCulling
{
	// inputIndices may be null
	size_t cull(size_t count, const uint32_t *inputIndices, const glm::vec4 *boundingSpheres, const glm::mat4 &viewProjection, uint32_t *resultIndices) noexcept;
}