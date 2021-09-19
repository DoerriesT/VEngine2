#pragma once
#include "volk.h"
#include <EASTL/fixed_hash_map.h>
#include "FramebufferDescriptionVk.h"

namespace gal
{
	bool operator==(const FramebufferDescriptionVk &lhs, const FramebufferDescriptionVk &rhs);
	struct FramebufferDescriptionHashVk { size_t operator()(const FramebufferDescriptionVk &value) const; };


	class FramebufferCacheVk
	{
	public:
		explicit FramebufferCacheVk(VkDevice device);
		FramebufferCacheVk(FramebufferCacheVk &) = delete;
		FramebufferCacheVk(FramebufferCacheVk &&) = delete;
		FramebufferCacheVk &operator= (const FramebufferCacheVk &) = delete;
		FramebufferCacheVk &operator= (const FramebufferCacheVk &&) = delete;
		~FramebufferCacheVk();
		VkFramebuffer getFramebuffer(const FramebufferDescriptionVk &framebufferDescription);
	private:
		eastl::fixed_hash_map<FramebufferDescriptionVk, VkFramebuffer, 64, 65, true, FramebufferDescriptionHashVk> m_framebuffers;
		VkDevice m_device;
	};
}