#include "CommandListVk.h"
#include "PipelineVk.h"
#include "ResourceVk.h"
#include "QueueVk.h"
#include <algorithm>
#include "assert.h"
#include "GraphicsDeviceVk.h"
#include "RenderPassDescriptionVk.h"
#include "FramebufferDescriptionVk.h"
#include "UtilityVk.h"

gal::CommandListVk::CommandListVk(VkCommandBuffer commandBuffer, GraphicsDeviceVk *device)
	:m_commandBuffer(commandBuffer),
	m_device(device)
{
}

void *gal::CommandListVk::getNativeHandle() const
{
	return m_commandBuffer;
}

void gal::CommandListVk::begin()
{
	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
}

void gal::CommandListVk::end()
{
	vkEndCommandBuffer(m_commandBuffer);
}

void gal::CommandListVk::bindPipeline(const GraphicsPipeline *pipeline)
{
	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)pipeline->getNativeHandle());
}

void gal::CommandListVk::bindPipeline(const ComputePipeline *pipeline)
{
	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, (VkPipeline)pipeline->getNativeHandle());
}

void gal::CommandListVk::setViewport(uint32_t firstViewport, uint32_t viewportCount, const Viewport *viewports)
{
	vkCmdSetViewport(m_commandBuffer, firstViewport, viewportCount, reinterpret_cast<const VkViewport *>(viewports));
}

void gal::CommandListVk::setScissor(uint32_t firstScissor, uint32_t scissorCount, const Rect *scissors)
{
	vkCmdSetScissor(m_commandBuffer, firstScissor, scissorCount, reinterpret_cast<const VkRect2D *>(scissors));
}

void gal::CommandListVk::setLineWidth(float lineWidth)
{
	vkCmdSetLineWidth(m_commandBuffer, lineWidth);
}

void gal::CommandListVk::setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	vkCmdSetDepthBias(m_commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

void gal::CommandListVk::setBlendConstants(const float blendConstants[4])
{
	vkCmdSetBlendConstants(m_commandBuffer, blendConstants);
}

void gal::CommandListVk::setDepthBounds(float minDepthBounds, float maxDepthBounds)
{
	vkCmdSetDepthBounds(m_commandBuffer, minDepthBounds, maxDepthBounds);
}

void gal::CommandListVk::setStencilCompareMask(StencilFaceFlags faceMask, uint32_t compareMask)
{
	vkCmdSetStencilCompareMask(m_commandBuffer, UtilityVk::translateStencilFaceFlags(faceMask), compareMask);
}

void gal::CommandListVk::setStencilWriteMask(StencilFaceFlags faceMask, uint32_t writeMask)
{
	vkCmdSetStencilWriteMask(m_commandBuffer, UtilityVk::translateStencilFaceFlags(faceMask), writeMask);
}

void gal::CommandListVk::setStencilReference(StencilFaceFlags faceMask, uint32_t reference)
{
	vkCmdSetStencilReference(m_commandBuffer, UtilityVk::translateStencilFaceFlags(faceMask), reference);
}

void gal::CommandListVk::bindDescriptorSets2(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets, uint32_t offsetCount, uint32_t *offsets)
{
	const auto *pipelineVk = dynamic_cast<const GraphicsPipelineVk *>(pipeline);
	assert(pipelineVk);

	uint32_t curDynamicBufferOffset = 0;
	constexpr uint32_t batchSize = 8;
	const VkPipelineLayout pipelineLayoutVk = pipelineVk->getLayout();
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t dynamicBufferCount = 0;
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSet setsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			auto *setVk = dynamic_cast<const DescriptorSetVk *>(sets[i * batchSize + j]);
			setsVk[j] = (VkDescriptorSet)setVk->getNativeHandle();
			dynamicBufferCount += setVk->getDynamicBufferCount();
		}
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutVk, firstSet + i * batchSize, countVk, setsVk, dynamicBufferCount, dynamicBufferCount ? offsets + curDynamicBufferOffset : nullptr);
		curDynamicBufferOffset += dynamicBufferCount;
	}
}

void gal::CommandListVk::bindDescriptorSets2(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets, uint32_t offsetCount, uint32_t *offsets)
{
	const auto *pipelineVk = dynamic_cast<const ComputePipelineVk *>(pipeline);
	assert(pipelineVk);

	uint32_t curDynamicBufferOffset = 0;
	constexpr uint32_t batchSize = 8;
	const VkPipelineLayout pipelineLayoutVk = pipelineVk->getLayout();
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t dynamicBufferCount = 0;
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSet setsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			auto *setVk = dynamic_cast<const DescriptorSetVk *>(sets[i * batchSize + j]);
			setsVk[j] = (VkDescriptorSet)setVk->getNativeHandle();
			dynamicBufferCount += setVk->getDynamicBufferCount();
		}
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutVk, firstSet + i * batchSize, countVk, setsVk, dynamicBufferCount, dynamicBufferCount ? offsets + curDynamicBufferOffset : nullptr);
		curDynamicBufferOffset += dynamicBufferCount;
	}
}

void gal::CommandListVk::bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType)
{
	const auto *bufferVk = dynamic_cast<const BufferVk *>(buffer);
	assert(bufferVk);

	vkCmdBindIndexBuffer(m_commandBuffer, (VkBuffer)bufferVk->getNativeHandle(), offset, UtilityVk::translate(indexType));
}

void gal::CommandListVk::bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer *const *buffers, uint64_t *offsets)
{
	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkBuffer buffersVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			buffersVk[j] = (VkBuffer)buffers[i * batchSize + j]->getNativeHandle();
		}
		vkCmdBindVertexBuffers(m_commandBuffer, firstBinding + i * batchSize, countVk, buffersVk, offsets + uint64_t(i) * uint64_t(batchSize));
	}
}

void gal::CommandListVk::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void gal::CommandListVk::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void gal::CommandListVk::drawIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndirect(m_commandBuffer, (VkBuffer)buffer->getNativeHandle(), offset, drawCount, stride);
}

void gal::CommandListVk::drawIndexedIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndexedIndirect(m_commandBuffer, (VkBuffer)buffer->getNativeHandle(), offset, drawCount, stride);
}

void gal::CommandListVk::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	vkCmdDispatch(m_commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void gal::CommandListVk::dispatchIndirect(const Buffer *buffer, uint64_t offset)
{
	vkCmdDispatchIndirect(m_commandBuffer, (VkBuffer)buffer->getNativeHandle(), offset);
}

void gal::CommandListVk::copyBuffer(const Buffer *srcBuffer, const Buffer *dstBuffer, uint32_t regionCount, const BufferCopy *regions)
{
	vkCmdCopyBuffer(m_commandBuffer, (VkBuffer)srcBuffer->getNativeHandle(), (VkBuffer)dstBuffer->getNativeHandle(), regionCount, reinterpret_cast<const VkBufferCopy *>(regions));
}

void gal::CommandListVk::copyImage(const Image *srcImage, const Image *dstImage, uint32_t regionCount, const ImageCopy *regions)
{
	constexpr uint32_t batchSize = 16;
	const VkImage srcImageVk = (VkImage)srcImage->getNativeHandle();
	const VkImage dstImageVk = (VkImage)dstImage->getNativeHandle();
	const VkImageAspectFlags srcAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(srcImage->getDescription().m_format));
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(dstImage->getDescription().m_format));
	const uint32_t iterations = (regionCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, regionCount - i * batchSize);
		VkImageCopy regionsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &region = regions[i * batchSize + j];
			regionsVk[j] =
			{
				{ srcAspectMask, region.m_srcMipLevel, region.m_srcBaseLayer, region.m_srcLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_srcOffset),
				{ dstAspectMask, region.m_dstMipLevel, region.m_dstBaseLayer, region.m_dstLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_dstOffset),
				*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
			};
		}
		vkCmdCopyImage(m_commandBuffer, srcImageVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, countVk, regionsVk);
	}
}

void gal::CommandListVk::copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions)
{
	constexpr uint32_t batchSize = 16;
	const VkBuffer srcBufferVk = (VkBuffer)srcBuffer->getNativeHandle();
	const VkImage dstImageVk = (VkImage)dstImage->getNativeHandle();

	const auto *bufferVk = dynamic_cast<const BufferVk *>(srcBuffer);
	assert(bufferVk);

	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(dstImage->getDescription().m_format));
	const uint32_t iterations = (regionCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, regionCount - i * batchSize);
		VkBufferImageCopy regionsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &region = regions[i * batchSize + j];
			regionsVk[j] =
			{
				region.m_bufferOffset,
				region.m_bufferRowLength,
				region.m_bufferImageHeight,
				{ dstAspectMask, region.m_imageMipLevel, region.m_imageBaseLayer, region.m_imageLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_offset),
				*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
			};
		}
		vkCmdCopyBufferToImage(m_commandBuffer, srcBufferVk, dstImageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, countVk, regionsVk);
	}
}

void gal::CommandListVk::copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions)
{
	constexpr uint32_t batchSize = 16;
	const VkImage srcImageVk = (VkImage)srcImage->getNativeHandle();
	const VkBuffer dstBufferVk = (VkBuffer)dstBuffer->getNativeHandle();


	const auto *bufferVk = dynamic_cast<const BufferVk *>(dstBuffer);
	assert(bufferVk);

	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(srcImage->getDescription().m_format));
	const uint32_t iterations = (regionCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, regionCount - i * batchSize);
		VkBufferImageCopy regionsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &region = regions[i * batchSize + j];
			regionsVk[j] =
			{
				region.m_bufferOffset,
				region.m_bufferRowLength,
				region.m_bufferImageHeight,
				{ dstAspectMask, region.m_imageMipLevel, region.m_imageBaseLayer, region.m_imageLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_offset),
				*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
			};
		}
		vkCmdCopyImageToBuffer(m_commandBuffer, srcImageVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBufferVk, countVk, regionsVk);
	}
}

void gal::CommandListVk::updateBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t dataSize, const void *data)
{

	vkCmdUpdateBuffer(m_commandBuffer, (VkBuffer)dstBuffer->getNativeHandle(), dstOffset, dataSize, data);
}

void gal::CommandListVk::fillBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t size, uint32_t data)
{
	vkCmdFillBuffer(m_commandBuffer, (VkBuffer)dstBuffer->getNativeHandle(), dstOffset, size, data);
}

void gal::CommandListVk::clearColorImage(const Image *image, const ClearColorValue *color, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	constexpr uint32_t batchSize = 8;
	const VkImage imageVk = (VkImage)image->getNativeHandle();
	const uint32_t iterations = (rangeCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t rangeCountVk = std::min(batchSize, rangeCount - i * batchSize);
		VkImageSubresourceRange rangesVk[batchSize];
		for (uint32_t j = 0; j < rangeCountVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { VK_IMAGE_ASPECT_COLOR_BIT, range.m_baseMipLevel, range.m_levelCount, range.m_baseArrayLayer, range.m_layerCount };
		}
		vkCmdClearColorImage(m_commandBuffer, imageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearColorValue *>(color), rangeCountVk, rangesVk);
	}
}

void gal::CommandListVk::clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	constexpr uint32_t batchSize = 8;
	const VkImage imageVk = (VkImage)image->getNativeHandle();
	const VkImageAspectFlags imageAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(image->getDescription().m_format));
	const uint32_t iterations = (rangeCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t rangeCountVk = std::min(batchSize, rangeCount - i * batchSize);
		VkImageSubresourceRange rangesVk[batchSize];
		for (uint32_t j = 0; j < rangeCountVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { imageAspectMask, range.m_baseMipLevel, range.m_levelCount, range.m_baseArrayLayer, range.m_layerCount };
		}
		vkCmdClearDepthStencilImage(m_commandBuffer, imageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearDepthStencilValue *>(depthStencil), rangeCountVk, rangesVk);
	}
}

void gal::CommandListVk::barrier(uint32_t count, const Barrier *barriers)
{
	constexpr uint32_t imageBarrierBatchSize = 16;
	constexpr uint32_t bufferBarrierBatchSize = 8;

	struct ResourceStateInfo
	{
		VkPipelineStageFlags m_stageMask;
		VkAccessFlags m_accessMask;
		VkImageLayout m_layout;
		bool m_readAccess;
		bool m_writeAccess;
	};

	auto getResourceStateInfo = [](ResourceState state, VkPipelineStageFlags stageFlags, bool isImage) -> ResourceStateInfo
	{
		switch (state)
		{
		case ResourceState::UNDEFINED:
			return { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, false, false };

		case ResourceState::READ_HOST:
			return { VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, isImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, true, false };

		case ResourceState::READ_DEPTH_STENCIL:
			return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, true, false };
		
		case ResourceState::READ_DEPTH_STENCIL_SHADER:
			return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, true, false };
		
		case ResourceState::READ_RESOURCE:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT, isImage ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_RW_RESOURCE:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT, isImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_CONSTANT_BUFFER:
			return { stageFlags, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_VERTEX_BUFFER:
			return { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_INDEX_BUFFER:
			return { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_INDIRECT_BUFFER:
			return { VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_TRANSFER:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, isImage ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		
		case ResourceState::READ_WRITE_HOST:
			return { VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, isImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, false, true };
		
		case ResourceState::READ_WRITE_RW_RESOURCE:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, isImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, true, true };
		
		case ResourceState::READ_WRITE_DEPTH_STENCIL:
			return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false, true };
		
		case ResourceState::WRITE_HOST:
			return { VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT, isImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, true, true };
		
		case ResourceState::WRITE_COLOR_ATTACHMENT:
			return { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false, true };
		
		case ResourceState::WRITE_RW_RESOURCE:
			return { stageFlags, VK_ACCESS_SHADER_WRITE_BIT, isImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED, false, true };
		
		case ResourceState::WRITE_TRANSFER:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, isImage ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, false, true };
		
		case ResourceState::CLEAR_RESOURCE:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, isImage ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, false, true };
		
		case ResourceState::PRESENT:
			return { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, true, false };
		
		default:
			assert(false);
			break;
		}
		return {};
	};

	uint32_t i = 0;

	while (i < count)
	{
		uint32_t imageBarrierCount = 0;
		uint32_t bufferBarrierCount = 0;

		VkImageMemoryBarrier imageBarriers[imageBarrierBatchSize];
		VkBufferMemoryBarrier bufferBarriers[bufferBarrierBatchSize];
		VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		for (; i < count && bufferBarrierCount < bufferBarrierBatchSize && imageBarrierCount < imageBarrierBatchSize; ++i)
		{
			const auto &barrier = barriers[i];
			assert(bool(barrier.m_image) != bool(barrier.m_buffer));

			// the vulkan backend currently does not support split barriers,
			// so we just ignore the first half and treat the second as a regular pipeline barrier
			if ((barrier.m_flags & BarrierFlags::BARRIER_BEGIN) != 0)
			{
				continue;
			}

			const auto beforeStateInfo = getResourceStateInfo(barrier.m_stateBefore, UtilityVk::translatePipelineStageFlags(barrier.m_stagesBefore), bool(barrier.m_image));
			const auto afterStateInfo = getResourceStateInfo(barrier.m_stateAfter, UtilityVk::translatePipelineStageFlags(barrier.m_stagesAfter), bool(barrier.m_image));

			const bool queueAcquire = (barrier.m_flags & BarrierFlags::QUEUE_OWNERSHIP_AQUIRE) != 0;
			const bool queueRelease = (barrier.m_flags & BarrierFlags::QUEUE_OWNERSHIP_RELEASE) != 0;

			const bool imageBarrierRequired = barrier.m_image && (beforeStateInfo.m_layout != afterStateInfo.m_layout || queueAcquire || queueRelease);
			const bool bufferBarrierRequired = barrier.m_buffer && (queueAcquire || queueRelease);
			const bool memoryBarrierRequired = beforeStateInfo.m_writeAccess && !imageBarrierRequired && !bufferBarrierRequired;
			const bool executionBarrierRequired = beforeStateInfo.m_writeAccess || afterStateInfo.m_writeAccess || memoryBarrierRequired || bufferBarrierRequired || imageBarrierRequired;

			const QueueVk *srcQueueVk = dynamic_cast<const QueueVk *>(barrier.m_srcQueue);
			const QueueVk *dstQueueVk = dynamic_cast<const QueueVk *>(barrier.m_dstQueue);

			if (imageBarrierRequired)
			{
				const auto &subResRange = barrier.m_imageSubresourceRange;
				const VkImageAspectFlags imageAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(barrier.m_image->getDescription().m_format));

				auto &imageBarrier = imageBarriers[imageBarrierCount++];
				imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarrier.srcAccessMask = queueAcquire ? 0 : beforeStateInfo.m_accessMask;
				imageBarrier.dstAccessMask = queueRelease ? 0 : afterStateInfo.m_accessMask;
				imageBarrier.oldLayout = beforeStateInfo.m_layout;
				imageBarrier.newLayout = afterStateInfo.m_layout;
				imageBarrier.srcQueueFamilyIndex = barrier.m_srcQueue ? srcQueueVk->m_queueFamily : VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.dstQueueFamilyIndex = barrier.m_dstQueue ? dstQueueVk->m_queueFamily : VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.image = (VkImage)barrier.m_image->getNativeHandle();
				imageBarrier.subresourceRange = { imageAspectMask, subResRange.m_baseMipLevel, subResRange.m_levelCount, subResRange.m_baseArrayLayer, subResRange.m_layerCount };
			}
			else if (bufferBarrierRequired)
			{
				auto &bufferBarrier = bufferBarriers[bufferBarrierCount++];
				bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
				bufferBarrier.srcAccessMask = queueAcquire ? 0 : beforeStateInfo.m_accessMask;
				bufferBarrier.dstAccessMask = queueRelease ? 0 : afterStateInfo.m_accessMask;
				bufferBarrier.srcQueueFamilyIndex = barrier.m_srcQueue ? srcQueueVk->m_queueFamily : VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = barrier.m_dstQueue ? dstQueueVk->m_queueFamily : VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = (VkBuffer)barrier.m_buffer->getNativeHandle();
				bufferBarrier.offset = 0;
				bufferBarrier.size = VK_WHOLE_SIZE;
			}

			if (memoryBarrierRequired)
			{
				memoryBarrier.srcAccessMask = beforeStateInfo.m_accessMask;
				memoryBarrier.dstAccessMask = afterStateInfo.m_accessMask;
			}

			if (executionBarrierRequired)
			{
				srcStages |= queueAcquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : beforeStateInfo.m_stageMask;
				dstStages |= queueRelease ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : afterStateInfo.m_stageMask;
			}
		}

		if (bufferBarrierCount || imageBarrierCount || memoryBarrier.srcAccessMask || srcStages != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT || dstStages != VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
		{
			vkCmdPipelineBarrier(m_commandBuffer, srcStages, dstStages, 0, 1, &memoryBarrier, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
		}
	}
}

void gal::CommandListVk::beginQuery(const QueryPool *queryPool, uint32_t query)
{
	vkCmdBeginQuery(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), query, 0);
}

void gal::CommandListVk::endQuery(const QueryPool *queryPool, uint32_t query)
{
	vkCmdEndQuery(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), query);
}

void gal::CommandListVk::resetQueryPool(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount)
{
	vkCmdResetQueryPool(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), firstQuery, queryCount);
}

void gal::CommandListVk::writeTimestamp(PipelineStageFlags pipelineStage, const QueryPool *queryPool, uint32_t query)
{
	vkCmdWriteTimestamp(m_commandBuffer, static_cast<VkPipelineStageFlagBits>(UtilityVk::translatePipelineStageFlags(pipelineStage)), (VkQueryPool)queryPool->getNativeHandle(), query);
}

void gal::CommandListVk::copyQueryPoolResults(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, const Buffer *dstBuffer, uint64_t dstOffset)
{
	vkCmdCopyQueryPoolResults(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), firstQuery, queryCount, (VkBuffer)dstBuffer->getNativeHandle(), dstOffset, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

void gal::CommandListVk::pushConstants(const GraphicsPipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values)
{
	const auto *pipelineVk = dynamic_cast<const GraphicsPipelineVk *>(pipeline);
	assert(pipelineVk);
	vkCmdPushConstants(m_commandBuffer, pipelineVk->getLayout(), UtilityVk::translateShaderStageFlags(stageFlags), offset, size, values);
}

void gal::CommandListVk::pushConstants(const ComputePipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values)
{
	const auto *pipelineVk = dynamic_cast<const ComputePipelineVk *>(pipeline);
	assert(pipelineVk);
	vkCmdPushConstants(m_commandBuffer, pipelineVk->getLayout(), UtilityVk::translateShaderStageFlags(stageFlags), offset, size, values);
}

void gal::CommandListVk::beginRenderPass(uint32_t colorAttachmentCount, ColorAttachmentDescription *colorAttachments, DepthStencilAttachmentDescription *depthStencilAttachment, const Rect &renderArea, bool rwTextureBufferAccess)
{
	assert(colorAttachmentCount <= 8);

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;

	VkClearValue clearValues[9] = {};
	VkImageView attachmentViews[9] = {};
	RenderPassDescriptionVk::ColorAttachmentDescriptionVk colorAttachmentDescsVk[8] = {};
	RenderPassDescriptionVk::DepthStencilAttachmentDescriptionVk depthStencilAttachmentDescVk = {};
	FramebufferDescriptionVk::AttachmentInfoVk fbAttachmentInfoVk[9] = {};

	uint32_t attachmentCount = 0;

	uint32_t framebufferWidth = -1;
	uint32_t framebufferHeight = -1;

	// fill out color attachment info structs
	for (uint32_t i = 0; i < colorAttachmentCount; ++i)
	{
		const auto &attachment = colorAttachments[i];
		const auto *image = attachment.m_imageView->getImage();
		const auto &imageDesc = image->getDescription();

		framebufferWidth = std::min(framebufferWidth, imageDesc.m_width);
		framebufferHeight = std::min(framebufferHeight, imageDesc.m_height);

		attachmentViews[i] = (VkImageView)attachment.m_imageView->getNativeHandle();
		clearValues[i].color = *reinterpret_cast<const VkClearColorValue *>(&attachment.m_clearValue);

		auto &attachmentDesc = colorAttachmentDescsVk[i];
		attachmentDesc = {};
		attachmentDesc.m_format = UtilityVk::translate(imageDesc.m_format);
		attachmentDesc.m_samples = static_cast<VkSampleCountFlagBits>(imageDesc.m_samples);
		attachmentDesc.m_loadOp = UtilityVk::translate(attachment.m_loadOp);
		attachmentDesc.m_storeOp = UtilityVk::translate(attachment.m_storeOp);

		const auto &viewDesc = attachment.m_imageView->getDescription();

		auto &fbAttachInfo = fbAttachmentInfoVk[i];
		fbAttachInfo = {};
		fbAttachInfo.m_flags = UtilityVk::translateImageCreateFlags(imageDesc.m_createFlags);
		fbAttachInfo.m_usage = UtilityVk::translateImageUsageFlags(imageDesc.m_usageFlags);
		fbAttachInfo.m_width = imageDesc.m_width;
		fbAttachInfo.m_height = imageDesc.m_height;
		fbAttachInfo.m_layerCount = viewDesc.m_layerCount;
		fbAttachInfo.m_format = attachmentDesc.m_format;

		++attachmentCount;
	}

	// fill out depth/stencil attachment info struct
	if (depthStencilAttachment)
	{
		const auto &attachment = *depthStencilAttachment;
		const auto *image = attachment.m_imageView->getImage();
		const auto &imageDesc = image->getDescription();

		framebufferWidth = std::min(framebufferWidth, imageDesc.m_width);
		framebufferHeight = std::min(framebufferHeight, imageDesc.m_height);

		attachmentViews[attachmentCount] = (VkImageView)attachment.m_imageView->getNativeHandle();
		clearValues[attachmentCount].depthStencil = *reinterpret_cast<const VkClearDepthStencilValue *>(&attachment.m_clearValue);

		auto &attachmentDesc = depthStencilAttachmentDescVk;
		attachmentDesc = {};
		attachmentDesc.m_format = UtilityVk::translate(imageDesc.m_format);
		attachmentDesc.m_samples = static_cast<VkSampleCountFlagBits>(imageDesc.m_samples);
		attachmentDesc.m_loadOp = UtilityVk::translate(attachment.m_loadOp);
		attachmentDesc.m_storeOp = UtilityVk::translate(attachment.m_storeOp);
		attachmentDesc.m_stencilLoadOp = UtilityVk::translate(attachment.m_stencilLoadOp);
		attachmentDesc.m_stencilStoreOp = UtilityVk::translate(attachment.m_stencilStoreOp);
		attachmentDesc.m_layout = attachment.m_readOnly ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		const auto &viewDesc = attachment.m_imageView->getDescription();

		auto &fbAttachInfo = fbAttachmentInfoVk[attachmentCount];
		fbAttachInfo = {};
		fbAttachInfo.m_flags = UtilityVk::translateImageCreateFlags(imageDesc.m_createFlags);
		fbAttachInfo.m_usage = UtilityVk::translateImageUsageFlags(imageDesc.m_usageFlags);
		fbAttachInfo.m_width = imageDesc.m_width;
		fbAttachInfo.m_height = imageDesc.m_height;
		fbAttachInfo.m_layerCount = viewDesc.m_layerCount;
		fbAttachInfo.m_format = attachmentDesc.m_format;

		++attachmentCount;
	}

	// get renderpass
	{
		RenderPassDescriptionVk renderPassDescription;
		renderPassDescription.setColorAttachments(colorAttachmentCount, colorAttachmentDescsVk);
		if (depthStencilAttachment)
		{
			renderPassDescription.setDepthStencilAttachment(depthStencilAttachmentDescVk);
		}
		renderPassDescription.finalize();

		renderPass = m_device->getRenderPass(renderPassDescription);
	}

	// get framebuffer
	{
		if (framebufferWidth == -1 || framebufferHeight == -1)
		{
			framebufferWidth = renderArea.m_offset.m_x + renderArea.m_extent.m_width;
			framebufferHeight = renderArea.m_offset.m_y + renderArea.m_extent.m_height;
		}

		FramebufferDescriptionVk framebufferDescription;
		framebufferDescription.setRenderPass(renderPass);
		framebufferDescription.setAttachments(attachmentCount, fbAttachmentInfoVk);
		framebufferDescription.setExtent(framebufferWidth, framebufferHeight, 1);
		framebufferDescription.finalize();

		framebuffer = m_device->getFramebuffer(framebufferDescription);
	}

	VkRenderPassAttachmentBeginInfo attachmentBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
	attachmentBeginInfo.attachmentCount = attachmentCount;
	attachmentBeginInfo.pAttachments = attachmentViews;

	VkRenderPassBeginInfo renderPassBeginInfoVk{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, &attachmentBeginInfo };
	renderPassBeginInfoVk.renderPass = renderPass;
	renderPassBeginInfoVk.framebuffer = framebuffer;
	renderPassBeginInfoVk.renderArea = *reinterpret_cast<const VkRect2D *>(&renderArea);
	renderPassBeginInfoVk.clearValueCount = attachmentCount;
	renderPassBeginInfoVk.pClearValues = clearValues;

	vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfoVk, VK_SUBPASS_CONTENTS_INLINE);
}

void gal::CommandListVk::endRenderPass()
{
	vkCmdEndRenderPass(m_commandBuffer);
}

void gal::CommandListVk::insertDebugLabel(const char *label)
{
	//if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsLabelEXT labelVk{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		labelVk.pLabelName = label;
		vkCmdInsertDebugUtilsLabelEXT(m_commandBuffer, &labelVk);
	}
}

void gal::CommandListVk::beginDebugLabel(const char *label)
{
	//if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsLabelEXT labelVk{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		labelVk.pLabelName = label;
		vkCmdBeginDebugUtilsLabelEXT(m_commandBuffer, &labelVk);
	}
}

void gal::CommandListVk::endDebugLabel()
{
	//if (g_vulkanDebugCallBackEnabled)
	{
		vkCmdEndDebugUtilsLabelEXT(m_commandBuffer);
	}
}