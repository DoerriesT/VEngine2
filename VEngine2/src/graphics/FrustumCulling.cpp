#include "FrustumCulling.h"
#include "job/ParallelFor.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/atomic.h>
#include "profiling/Profiling.h"

FrustumCulling::FrustumInfo FrustumCulling::createFrustumInfo(const glm::mat4 &viewProjection) noexcept
{
	FrustumInfo result{};

	// extract frustum planes from matrix
	glm::mat4 transposed = glm::transpose(viewProjection);
	result.m_planes[0] = -(transposed[3] + transposed[0]);	// left
	result.m_planes[1] = -(transposed[3] - transposed[0]);	// right
	result.m_planes[2] = -(transposed[3] + transposed[1]);	// bottom
	result.m_planes[3] = -(transposed[3] - transposed[1]);	// top
	result.m_planes[4] = -(transposed[3] - transposed[2]);	// far
	result.m_planes[5] = -(transposed[2]);			// near

	for (auto &p : result.m_planes)
	{
		p /= glm::length(glm::vec3(p));
	}

	return result;
}

bool FrustumCulling::cull(const FrustumInfo &frustum, const glm::vec4 &boundingSphere) noexcept
{
	bool culled = false;
	for (const auto &p : frustum.m_planes)
	{
		if (glm::dot(glm::vec4(boundingSphere.x, boundingSphere.y, boundingSphere.z, 1.0f), p) > boundingSphere.w)
		{
			culled = true;
			break;
		}
	}

	return culled;
}

size_t FrustumCulling::cull(size_t count, const uint32_t *inputIndices, const glm::vec4 *boundingSpheres, const glm::mat4 &viewProjection, uint32_t *resultIndices) noexcept
{
	PROFILING_ZONE_SCOPED;

	auto frustum = createFrustumInfo(viewProjection);

	eastl::atomic<uint32_t> resultOffset = 0;

	job::parallelFor(count, 32, [&](size_t startIdx, size_t endIdx)
		{
			PROFILING_ZONE_SCOPED_N("Frustum Culling Job");
			for (size_t j = startIdx; j < endIdx; ++j)
			{
				const auto &bsphere = boundingSpheres[j];
				bool culled = false;
				for (const auto &p : frustum.m_planes)
				{
					if (glm::dot(glm::vec4(bsphere.x, bsphere.y, bsphere.z, 1.0f), p) > bsphere.w)
					{
						culled = true;
						break;
					}
				}
				if (!culled)
				{
					resultIndices[resultOffset.fetch_add(1)] = inputIndices ? inputIndices[j] : static_cast<uint32_t>(j);
				}
			}
		});

	return static_cast<size_t>(resultOffset.load());
}
