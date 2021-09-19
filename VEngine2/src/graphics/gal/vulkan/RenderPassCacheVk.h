#pragma once
#include "volk.h"
#include <EASTL/fixed_hash_map.h>
#include "RenderPassDescriptionVk.h"

namespace gal
{
	bool operator==(const RenderPassDescriptionVk &lhs, const RenderPassDescriptionVk &rhs);
	struct RenderPassDescriptionHashVk { size_t operator()(const RenderPassDescriptionVk &value) const; };


	class RenderPassCacheVk
	{
	public:
		explicit RenderPassCacheVk(VkDevice device);
		RenderPassCacheVk(RenderPassCacheVk &) = delete;
		RenderPassCacheVk(RenderPassCacheVk &&) = delete;
		RenderPassCacheVk &operator= (const RenderPassCacheVk &) = delete;
		RenderPassCacheVk &operator= (const RenderPassCacheVk &&) = delete;
		~RenderPassCacheVk();
		VkRenderPass getRenderPass(const RenderPassDescriptionVk &renderPassDescription);
	private:
		eastl::fixed_hash_map<RenderPassDescriptionVk, VkRenderPass, 64, 65, true, RenderPassDescriptionHashVk> m_renderPasses;
		VkDevice m_device;
	};
}