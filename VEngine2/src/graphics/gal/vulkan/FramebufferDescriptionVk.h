#pragma once
#include "volk.h"

namespace gal
{
	struct FramebufferDescriptionVk
	{
	public:
		struct AttachmentInfoVk
		{
			VkImageCreateFlags m_flags;
			VkImageUsageFlags m_usage;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_layerCount;
			VkFormat m_format;
		};

		explicit FramebufferDescriptionVk();
		void setRenderPass(VkRenderPass renderPass);
		void setAttachments(uint32_t count, const AttachmentInfoVk *attachmentInfos);
		void setExtent(uint32_t width, uint32_t height, uint32_t layers);
		void finalize();

		AttachmentInfoVk m_attachmentInfos[9];
		VkRenderPass m_renderPass;
		uint32_t m_attachmentCount;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_layers;
		size_t m_hashValue;
	};
}