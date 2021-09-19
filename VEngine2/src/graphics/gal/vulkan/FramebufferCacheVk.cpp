#include "FramebufferCacheVk.h"
#include "UtilityVk.h"
#include "assert.h"
#include "Utility/Utility.h"

gal::FramebufferDescriptionVk::FramebufferDescriptionVk()
{
	memset(this, 0, sizeof(*this));
}

void gal::FramebufferDescriptionVk::setRenderPass(VkRenderPass renderPass)
{
	m_renderPass = renderPass;
}

void gal::FramebufferDescriptionVk::setAttachments(uint32_t count, const AttachmentInfoVk *attachmentInfos)
{
	assert(count <= 9);
	memcpy(m_attachmentInfos, attachmentInfos, sizeof(m_attachmentInfos[0]) * count);
	m_attachmentCount = count;
}

void gal::FramebufferDescriptionVk::setExtent(uint32_t width, uint32_t height, uint32_t layers)
{
	m_width = width;
	m_height = height;
	m_layers = layers;
}

void gal::FramebufferDescriptionVk::finalize()
{
	m_hashValue = 0;
	// since m_hashValue is the last member and always 0 before hashing and >= 4 byte,
	// we can just ignore the last 3 byte if the sizeof(*this) is not divisible by 4.
	for (size_t i = 0; (i + 4) <= sizeof(*this); i += 4)
	{
		uint32_t word = *reinterpret_cast<const uint32_t *>(reinterpret_cast<const char *>(this) + i);
		util::hashCombine(m_hashValue, word);
	}
}

bool gal::operator==(const FramebufferDescriptionVk &lhs, const FramebufferDescriptionVk &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

size_t gal::FramebufferDescriptionHashVk::operator()(const FramebufferDescriptionVk &value) const
{
	return value.m_hashValue;
}

gal::FramebufferCacheVk::FramebufferCacheVk(VkDevice device)
	:m_device(device)
{
}

gal::FramebufferCacheVk::~FramebufferCacheVk()
{
	for (auto &elem : m_framebuffers)
	{
		vkDestroyFramebuffer(m_device, elem.second, nullptr);
	}
}

VkFramebuffer gal::FramebufferCacheVk::getFramebuffer(const FramebufferDescriptionVk &framebufferDescription)
{
	// get framebuffer from cache or create a new one
	VkFramebuffer &framebuffer = m_framebuffers[framebufferDescription];

	// framebuffer does not exist yet -> create it
	if (framebuffer == VK_NULL_HANDLE)
	{
		// fill out image infos
		VkFramebufferAttachmentImageInfo imageInfos[9];
		for (size_t i = 0; i < framebufferDescription.m_attachmentCount; ++i)
		{
			const auto &info = framebufferDescription.m_attachmentInfos[i];
			auto &infoVk = imageInfos[i];
			infoVk = { VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO };
			infoVk.flags = info.m_flags;
			infoVk.usage = info.m_usage;
			infoVk.width = info.m_width;
			infoVk.height = info.m_height;
			infoVk.layerCount = info.m_layerCount;
			infoVk.viewFormatCount = 1;
			infoVk.pViewFormats = &info.m_format;
		}

		VkFramebufferAttachmentsCreateInfo attachmentsCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO };
		attachmentsCreateInfo.attachmentImageInfoCount = framebufferDescription.m_attachmentCount;
		attachmentsCreateInfo.pAttachmentImageInfos = imageInfos;

		VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, &attachmentsCreateInfo };
		framebufferCreateInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
		framebufferCreateInfo.renderPass = framebufferDescription.m_renderPass;
		framebufferCreateInfo.attachmentCount = framebufferDescription.m_attachmentCount;
		framebufferCreateInfo.pAttachments = nullptr;
		framebufferCreateInfo.width = framebufferDescription.m_width;
		framebufferCreateInfo.height = framebufferDescription.m_height;
		framebufferCreateInfo.layers = framebufferDescription.m_layers;

		UtilityVk::checkResult(vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &framebuffer), "Failed to create Framebuffer!");
	}

	return framebuffer;
}
