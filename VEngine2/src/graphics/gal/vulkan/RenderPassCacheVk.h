#pragma once
#include "volk.h"
#include <EASTL/hash_map.h>
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
		eastl::hash_map<RenderPassDescriptionVk, VkRenderPass, RenderPassDescriptionHashVk> m_renderPasses;
		VkDevice m_device;
	};
}