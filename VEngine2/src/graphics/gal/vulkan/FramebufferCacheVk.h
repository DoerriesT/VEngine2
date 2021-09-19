#pragma once
#include "volk.h"
#include <EASTL/hash_map.h>
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
		eastl::hash_map<FramebufferDescriptionVk, VkFramebuffer, FramebufferDescriptionHashVk> m_framebuffers;
		VkDevice m_device;
	};
}