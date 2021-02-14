#pragma once
#include "volk.h"
#include "Graphics/gal/GraphicsAbstractionLayer.h"

namespace gal
{
	namespace UtilityVk
	{
		VkResult checkResult(VkResult result, const char *errorMsg = nullptr, bool exitOnError = true);
		VkImageAspectFlags getImageAspectMask(VkFormat format);
		VkComponentSwizzle translate(ComponentSwizzle swizzle);
		VkFilter translate(Filter filter);
		VkSamplerMipmapMode translate(SamplerMipmapMode mipmapMode);
		VkSamplerAddressMode translate(SamplerAddressMode addressMode);
		VkBorderColor translate(BorderColor borderColor);
		VkIndexType translate(IndexType indexType);
		VkQueryType translate(QueryType queryType);
		VkAttachmentLoadOp translate(AttachmentLoadOp loadOp);
		VkAttachmentStoreOp translate(AttachmentStoreOp storeOp);
		VkVertexInputRate translate(VertexInputRate inputRate);
		VkFormat translate(Format format);
		VkImageViewType translate(ImageViewType imageViewType);
		VkImageType translate(ImageType imageType);
		VkPrimitiveTopology translate(PrimitiveTopology primitiveTopology);
		VkPolygonMode translate(PolygonMode polygonMode);
		VkFrontFace translate(FrontFace frontFace);
		VkCompareOp translate(CompareOp compareOp);
		VkStencilOp translate(StencilOp stencilOp);
		VkLogicOp translate(LogicOp logicOp);
		VkBlendFactor translate(BlendFactor blendFactor);
		VkBlendOp translate(BlendOp blendOp);
		VkShaderStageFlags translateShaderStageFlags(ShaderStageFlags flags);
		VkMemoryPropertyFlags translateMemoryPropertyFlags(MemoryPropertyFlags flags);
		VkQueryPipelineStatisticFlags translateQueryPipelineStatisticFlags(QueryPipelineStatisticFlags flags);
		VkStencilFaceFlags translateStencilFaceFlags(StencilFaceFlags flags);
		VkPipelineStageFlags translatePipelineStageFlags(PipelineStageFlags flags);
		VkImageUsageFlags translateImageUsageFlags(ImageUsageFlags flags);
		VkImageCreateFlags translateImageCreateFlags(ImageCreateFlags flags);
		VkBufferUsageFlags translateBufferUsageFlags(BufferUsageFlags flags);
		VkBufferCreateFlags translateBufferCreateFlags(BufferCreateFlags flags);
		VkCullModeFlags translateCullModeFlags(CullModeFlags flags);
		VkColorComponentFlags translateColorComponentFlags(ColorComponentFlags flags);
		VkQueryResultFlags translateQueryResultFlags(QueryResultFlags flags);
		void translateDynamicStateFlags(DynamicStateFlags flags, uint32_t &stateCount, VkDynamicState states[9]);
		VkDescriptorBindingFlags translateDescriptorBindingFlags(DescriptorBindingFlags flags);
	}
}