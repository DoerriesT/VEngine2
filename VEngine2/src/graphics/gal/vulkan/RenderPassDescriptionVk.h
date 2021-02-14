#pragma once
#include "volk.h"

namespace gal
{
	struct RenderPassDescriptionVk
	{
	public:
		struct ColorAttachmentDescriptionVk
		{
			VkFormat m_format;
			VkSampleCountFlagBits m_samples;
			VkAttachmentLoadOp m_loadOp;
			VkAttachmentStoreOp m_storeOp;
		};

		struct DepthStencilAttachmentDescriptionVk
		{
			VkFormat m_format;
			VkSampleCountFlagBits m_samples;
			VkAttachmentLoadOp m_loadOp;
			VkAttachmentStoreOp m_storeOp;
			VkAttachmentLoadOp m_stencilLoadOp;
			VkAttachmentStoreOp m_stencilStoreOp;
			VkImageLayout m_layout;
		};

		explicit RenderPassDescriptionVk();
		void setColorAttachments(uint32_t count, const ColorAttachmentDescriptionVk *colorAttachments);
		void setDepthStencilAttachment(const DepthStencilAttachmentDescriptionVk &depthStencilAttachment);
		void finalize();

		ColorAttachmentDescriptionVk m_colorAttachments[8];
		DepthStencilAttachmentDescriptionVk m_depthStencilAttachment;
		uint32_t m_colorAttachmentCount;
		VkBool32 m_depthStencilAttachmentPresent;
		size_t m_hashValue;
	};
}