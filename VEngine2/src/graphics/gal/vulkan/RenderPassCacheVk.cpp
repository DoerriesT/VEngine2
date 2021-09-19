#include "RenderPassCacheVk.h"
#include "GraphicsDeviceVk.h"
#include "UtilityVk.h"
#include <assert.h>
#include "utility/Utility.h"


gal::RenderPassDescriptionVk::RenderPassDescriptionVk()
{
	memset(this, 0, sizeof(*this));
}

void gal::RenderPassDescriptionVk::setColorAttachments(uint32_t count, const ColorAttachmentDescriptionVk *colorAttachments)
{
	assert(count <= 8);
	memcpy(m_colorAttachments, colorAttachments, sizeof(colorAttachments[0]) * count);
	m_colorAttachmentCount = count;
}

void gal::RenderPassDescriptionVk::setDepthStencilAttachment(const DepthStencilAttachmentDescriptionVk &depthStencilAttachment)
{
	m_depthStencilAttachment = depthStencilAttachment;
	m_depthStencilAttachmentPresent = VK_TRUE;
}

void gal::RenderPassDescriptionVk::finalize()
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

bool gal::operator==(const RenderPassDescriptionVk &lhs, const RenderPassDescriptionVk &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

size_t gal::RenderPassDescriptionHashVk::operator()(const RenderPassDescriptionVk &value) const
{
	return value.m_hashValue;
}

gal::RenderPassCacheVk::RenderPassCacheVk(VkDevice device)
	:m_device(device)
{
}

gal::RenderPassCacheVk::~RenderPassCacheVk()
{
	for (auto &elem : m_renderPasses)
	{
		vkDestroyRenderPass(m_device, elem.second, nullptr);
	}
}

VkRenderPass gal::RenderPassCacheVk::getRenderPass(const RenderPassDescriptionVk &renderPassDescription)
{
	// get renderPass from cache or create a new one
	VkRenderPass &pass = m_renderPasses[renderPassDescription];

	// renderPass does not exist yet -> create it
	if (pass == VK_NULL_HANDLE)
	{
		uint32_t attachmentCount = 0;
		VkAttachmentDescription attachmentDescriptions[9] = {};
		VkAttachmentReference colorAttachmentReferences[8] = {};
		VkAttachmentReference depthStencilAttachmentReference = {};

		// fill out color attachment info structs
		for (uint32_t i = 0; i < renderPassDescription.m_colorAttachmentCount; ++i)
		{
			const auto &attachment = renderPassDescription.m_colorAttachments[i];

			colorAttachmentReferences[i] = { i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			auto &attachmentDesc = attachmentDescriptions[i];
			attachmentDesc = {};
			attachmentDesc.format = attachment.m_format;
			attachmentDesc.samples = attachment.m_samples;
			attachmentDesc.loadOp = attachment.m_loadOp;
			attachmentDesc.storeOp = attachment.m_storeOp;
			attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			++attachmentCount;
		}

		// fill out depth/stencil attachment info struct
		if (renderPassDescription.m_depthStencilAttachmentPresent)
		{
			const auto &attachment = renderPassDescription.m_depthStencilAttachment;

			depthStencilAttachmentReference = { attachmentCount, attachment.m_layout };

			auto &attachmentDesc = attachmentDescriptions[attachmentCount];
			attachmentDesc = {};
			attachmentDesc.format = attachment.m_format;
			attachmentDesc.samples = attachment.m_samples;
			attachmentDesc.loadOp = attachment.m_loadOp;
			attachmentDesc.storeOp = attachment.m_storeOp;
			attachmentDesc.stencilLoadOp = attachment.m_stencilLoadOp;
			attachmentDesc.stencilStoreOp = attachment.m_stencilStoreOp;
			attachmentDesc.initialLayout = attachment.m_layout;
			attachmentDesc.finalLayout = attachment.m_layout;

			++attachmentCount;
		}

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.colorAttachmentCount = renderPassDescription.m_colorAttachmentCount;
		subpassDescription.pColorAttachments = renderPassDescription.m_colorAttachmentCount > 0 ? colorAttachmentReferences : nullptr;
		subpassDescription.pResolveAttachments = nullptr;
		subpassDescription.pDepthStencilAttachment = renderPassDescription.m_depthStencilAttachmentPresent ? &depthStencilAttachmentReference : nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;

		VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		renderPassInfo.attachmentCount = attachmentCount;
		renderPassInfo.pAttachments = attachmentDescriptions;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = 0;
		renderPassInfo.pDependencies = nullptr;

		UtilityVk::checkResult(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &pass), "Failed to create RenderPass!");
	}

	return pass;
}
