#include "FrustumCulling.h"
#include "job/ParallelFor.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/atomic.h>

size_t FrustumCulling::cull(size_t count, const uint32_t *inputIndices, const glm::vec4 *boundingSpheres, const glm::mat4 &viewProjection, uint32_t *resultIndices) noexcept
{
	// extract frustum planes from matrix
	glm::mat4 transposed = glm::transpose(viewProjection);
	glm::vec4 planes[6];
	planes[0] = -(transposed[3] + transposed[0]);	// left
	planes[1] = -(transposed[3] - transposed[0]);	// right
	planes[2] = -(transposed[3] + transposed[1]);	// bottom
	planes[3] = -(transposed[3] - transposed[1]);	// top
	planes[4] = -(transposed[3] - transposed[2]);	// far
	planes[5] = -(transposed[2]);			// near

	for (auto &p : planes)
	{
		p /= glm::length(glm::vec3(p));
	}

	eastl::atomic<uint32_t> resultOffset = 0;

	job::parallelFor(count, 32, [&](size_t startIdx, size_t endIdx)
		{
			for (size_t j = startIdx; j < endIdx; ++j)
			{
				const auto &bsphere = boundingSpheres[j];
				bool culled = false;
				for (const auto &p : planes)
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
