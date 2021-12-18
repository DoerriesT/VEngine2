#include "CommandListVk.h"
#include "PipelineVk.h"
#include "ResourceVk.h"
#include "QueueVk.h"
#include <assert.h>
#include "GraphicsDeviceVk.h"
#include "RenderPassDescriptionVk.h"
#include "FramebufferDescriptionVk.h"
#include "UtilityVk.h"
#include "utility/allocator/DefaultAllocator.h"
#include "../Initializers.h"

namespace
{
	struct ResourceStateInfo
	{
		VkPipelineStageFlags m_stageMask;
		VkAccessFlags m_accessMask;
		VkImageLayout m_layout;
		bool m_readAccess;
		bool m_writeAccess;
	};

	static ResourceStateInfo getResourceStateInfo(gal::ResourceState state, VkPipelineStageFlags stageFlags, bool isImage, gal::Format imageFormat)
	{
		using namespace gal;

		ResourceStateInfo result{};

		if (state == ResourceState::UNDEFINED)
		{
			result = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, false, false };
			return result;
		}

		if ((state & ResourceState::READ_RESOURCE) != 0)
		{
			result.m_stageMask |= stageFlags;
			result.m_accessMask |= VK_ACCESS_SHADER_READ_BIT;
			result.m_layout = Initializers::isDepthFormat(imageFormat) || Initializers::isStencilFormat(imageFormat) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::READ_DEPTH_STENCIL) != 0)
		{
			assert(isImage);
			result.m_stageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			result.m_accessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::READ_CONSTANT_BUFFER) != 0)
		{
			assert(!isImage);
			result.m_stageMask |= stageFlags;
			result.m_accessMask |= VK_ACCESS_UNIFORM_READ_BIT;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::READ_VERTEX_BUFFER) != 0)
		{
			assert(!isImage);
			result.m_stageMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			result.m_accessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::READ_INDEX_BUFFER) != 0)
		{
			assert(!isImage);
			result.m_stageMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			result.m_accessMask |= VK_ACCESS_INDEX_READ_BIT;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::READ_INDIRECT_BUFFER) != 0)
		{
			assert(!isImage);
			result.m_stageMask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
			result.m_accessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::READ_TRANSFER) != 0)
		{
			assert(!isImage || state == ResourceState::READ_TRANSFER); // READ_TRANSFER is an exclusive state on image resources
			result.m_stageMask |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			result.m_accessMask |= VK_ACCESS_TRANSFER_READ_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			result.m_readAccess = true;
		}

		if ((state & ResourceState::WRITE_DEPTH_STENCIL) != 0)
		{
			assert(isImage);
			assert(state == ResourceState::WRITE_DEPTH_STENCIL);
			result.m_stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			result.m_accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			result.m_writeAccess = true;
		}

		if ((state & ResourceState::WRITE_COLOR_ATTACHMENT) != 0)
		{
			assert(isImage);
			assert(state == ResourceState::WRITE_COLOR_ATTACHMENT);
			result.m_stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			result.m_accessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			result.m_writeAccess = true;
		}

		if ((state & ResourceState::WRITE_TRANSFER) != 0)
		{
			assert(state == ResourceState::WRITE_TRANSFER);
			result.m_stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			result.m_accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			result.m_writeAccess = true;
		}

		if ((state & ResourceState::CLEAR_RESOURCE) != 0)
		{
			assert(state == ResourceState::CLEAR_RESOURCE);
			result.m_stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			result.m_accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			result.m_writeAccess = true;
		}

		if ((state & ResourceState::RW_RESOURCE) != 0)
		{
			assert(state == ResourceState::RW_RESOURCE);
			result.m_stageMask = stageFlags;
			result.m_accessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_GENERAL;
			result.m_readAccess = true;
			result.m_writeAccess = true;
		}

		if ((state & ResourceState::RW_RESOURCE_READ_ONLY) != 0)
		{
			assert(state == ResourceState::RW_RESOURCE_READ_ONLY);
			result.m_stageMask = stageFlags;
			result.m_accessMask = VK_ACCESS_SHADER_READ_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_GENERAL;
			result.m_readAccess = true;
			result.m_writeAccess = false;
		}

		if ((state & ResourceState::RW_RESOURCE_WRITE_ONLY) != 0)
		{
			assert(state == ResourceState::RW_RESOURCE_WRITE_ONLY);
			result.m_stageMask = stageFlags;
			result.m_accessMask = VK_ACCESS_SHADER_WRITE_BIT;
			result.m_layout = VK_IMAGE_LAYOUT_GENERAL;
			result.m_readAccess = false;
			result.m_writeAccess = true;
		}

		if ((state & ResourceState::PRESENT) != 0)
		{
			assert(isImage);
			assert(state == ResourceState::PRESENT);
			result.m_stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			result.m_accessMask = 0;
			result.m_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			result.m_readAccess = true;
			result.m_writeAccess = false;
		}

		result.m_layout = isImage ? result.m_layout : VK_IMAGE_LAYOUT_UNDEFINED;

		return result;
	};

}

gal::CommandListVk::CommandListVk(VkCommandBuffer commandBuffer, GraphicsDeviceVk *device) noexcept
	:m_commandBuffer(commandBuffer),
	m_device(device),
	m_scratchMemoryAllocation(DefaultAllocator::get(), DefaultAllocator::get()->allocate(k_scratchMemorySize), k_scratchMemorySize),
	m_linearAllocator((char *)m_scratchMemoryAllocation.getAllocation(), k_scratchMemorySize, "CommandListVk Linear Allocator")
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
	const auto *pipelineVk = dynamic_cast<const GraphicsPipelineVk *>(pipeline);
	assert(pipelineVk);

	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)pipeline->getNativeHandle());
	pipelineVk->bindStaticSamplerSet(m_commandBuffer);
}

void gal::CommandListVk::bindPipeline(const ComputePipeline *pipeline)
{
	const auto *pipelineVk = dynamic_cast<const ComputePipelineVk *>(pipeline);
	assert(pipelineVk);

	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, (VkPipeline)pipeline->getNativeHandle());
	pipelineVk->bindStaticSamplerSet(m_commandBuffer);
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

void gal::CommandListVk::bindDescriptorSets(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets, uint32_t offsetCount, uint32_t *offsets)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const auto *pipelineVk = dynamic_cast<const GraphicsPipelineVk *>(pipeline);
	assert(pipelineVk);

	VkPipelineLayout pipelineLayoutVk = pipelineVk->getLayout();

	VkDescriptorSet *setsVk = linearAllocatorFrame.allocateArray<VkDescriptorSet>(count);

	for (size_t i = 0; i < count; ++i)
	{
		setsVk[i] = (VkDescriptorSet)sets[i]->getNativeHandle();
	}

	vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutVk, firstSet, count, setsVk, offsetCount, offsets);
}

void gal::CommandListVk::bindDescriptorSets(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets, uint32_t offsetCount, uint32_t *offsets)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const auto *pipelineVk = dynamic_cast<const ComputePipelineVk *>(pipeline);
	assert(pipelineVk);

	VkPipelineLayout pipelineLayoutVk = pipelineVk->getLayout();

	VkDescriptorSet *setsVk = linearAllocatorFrame.allocateArray<VkDescriptorSet>(count);

	for (size_t i = 0; i < count; ++i)
	{
		setsVk[i] = (VkDescriptorSet)sets[i]->getNativeHandle();
	}

	vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutVk, firstSet, count, setsVk, offsetCount, offsets);
}

void gal::CommandListVk::bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType)
{
	const auto *bufferVk = dynamic_cast<const BufferVk *>(buffer);
	assert(bufferVk);

	vkCmdBindIndexBuffer(m_commandBuffer, (VkBuffer)bufferVk->getNativeHandle(), offset, UtilityVk::translate(indexType));
}

void gal::CommandListVk::bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer *const *buffers, uint64_t *offsets)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	VkBuffer *buffersVk = linearAllocatorFrame.allocateArray<VkBuffer>(count);

	for (size_t i = 0; i < count; ++i)
	{
		buffersVk[i] = (VkBuffer)buffers[i]->getNativeHandle();
	}

	vkCmdBindVertexBuffers(m_commandBuffer, firstBinding, count, buffersVk, offsets);
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
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const VkImage srcImageVk = (VkImage)srcImage->getNativeHandle();
	const VkImage dstImageVk = (VkImage)dstImage->getNativeHandle();
	const VkImageAspectFlags srcAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(srcImage->getDescription().m_format));
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(dstImage->getDescription().m_format));

	VkImageCopy *regionsVk = linearAllocatorFrame.allocateArray<VkImageCopy>(regionCount);

	for (size_t i = 0; i < regionCount; ++i)
	{
		const auto &region = regions[i];
		regionsVk[i] =
		{
			{ srcAspectMask, region.m_srcMipLevel, region.m_srcBaseLayer, region.m_srcLayerCount },
			*reinterpret_cast<const VkOffset3D *>(&region.m_srcOffset),
			{ dstAspectMask, region.m_dstMipLevel, region.m_dstBaseLayer, region.m_dstLayerCount },
			*reinterpret_cast<const VkOffset3D *>(&region.m_dstOffset),
			*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
		};
	}

	vkCmdCopyImage(m_commandBuffer, srcImageVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, regionsVk);
}

void gal::CommandListVk::copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const VkBuffer srcBufferVk = (VkBuffer)srcBuffer->getNativeHandle();
	const VkImage dstImageVk = (VkImage)dstImage->getNativeHandle();
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(dstImage->getDescription().m_format));

	VkBufferImageCopy *regionsVk = linearAllocatorFrame.allocateArray<VkBufferImageCopy>(regionCount);

	for (size_t i = 0; i < regionCount; ++i)
	{
		const auto &region = regions[i];
		regionsVk[i] =
		{
			region.m_bufferOffset,
			region.m_bufferRowLength,
			region.m_bufferImageHeight,
			{ dstAspectMask, region.m_imageMipLevel, region.m_imageBaseLayer, region.m_imageLayerCount },
			*reinterpret_cast<const VkOffset3D *>(&region.m_offset),
			*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
		};
	}

	vkCmdCopyBufferToImage(m_commandBuffer, srcBufferVk, dstImageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regionCount, regionsVk);
}

void gal::CommandListVk::copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const VkImage srcImageVk = (VkImage)srcImage->getNativeHandle();
	const VkBuffer dstBufferVk = (VkBuffer)dstBuffer->getNativeHandle();
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(srcImage->getDescription().m_format));

	VkBufferImageCopy *regionsVk = linearAllocatorFrame.allocateArray<VkBufferImageCopy>(regionCount);

	for (size_t i = 0; i < regionCount; ++i)
	{
		const auto &region = regions[i];
		regionsVk[i] =
		{
			region.m_bufferOffset,
			region.m_bufferRowLength,
			region.m_bufferImageHeight,
			{ dstAspectMask, region.m_imageMipLevel, region.m_imageBaseLayer, region.m_imageLayerCount },
			*reinterpret_cast<const VkOffset3D *>(&region.m_offset),
			*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
		};
	}

	vkCmdCopyImageToBuffer(m_commandBuffer, srcImageVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBufferVk, regionCount, regionsVk);
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
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const VkImage imageVk = (VkImage)image->getNativeHandle();
	VkImageSubresourceRange *rangesVk = linearAllocatorFrame.allocateArray<VkImageSubresourceRange>(rangeCount);

	for (size_t i = 0; i < rangeCount; ++i)
	{
		const auto &range = ranges[i];
		rangesVk[i] = { VK_IMAGE_ASPECT_COLOR_BIT, range.m_baseMipLevel, range.m_levelCount, range.m_baseArrayLayer, range.m_layerCount };
	}

	vkCmdClearColorImage(m_commandBuffer, imageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearColorValue *>(color), rangeCount, rangesVk);
}

void gal::CommandListVk::clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	const VkImage imageVk = (VkImage)image->getNativeHandle();
	const VkImageAspectFlags imageAspectMask = UtilityVk::getImageAspectMask(UtilityVk::translate(image->getDescription().m_format));
	VkImageSubresourceRange *rangesVk = linearAllocatorFrame.allocateArray<VkImageSubresourceRange>(rangeCount);

	for (size_t i = 0; i < rangeCount; ++i)
	{
		const auto &range = ranges[i];
		rangesVk[i] = { imageAspectMask, range.m_baseMipLevel, range.m_levelCount, range.m_baseArrayLayer, range.m_layerCount };
	}

	vkCmdClearDepthStencilImage(m_commandBuffer, imageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearDepthStencilValue *>(depthStencil), rangeCount, rangesVk);
}

void gal::CommandListVk::barrier(uint32_t count, const Barrier *barriers)
{
	LinearAllocatorFrame linearAllocatorFrame(&m_linearAllocator);

	uint32_t imageBarrierCount = 0;
	uint32_t bufferBarrierCount = 0;

	VkImageMemoryBarrier *imageBarriers = linearAllocatorFrame.allocateArray<VkImageMemoryBarrier>(count);
	VkBufferMemoryBarrier *bufferBarriers = linearAllocatorFrame.allocateArray<VkBufferMemoryBarrier>(count);
	VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	for (size_t i = 0; i < count; ++i)
	{
		const auto &barrier = barriers[i];
		assert(bool(barrier.m_image) != bool(barrier.m_buffer));

		// the vulkan backend currently does not support split barriers,
		// so we just ignore the first half and treat the second as a regular pipeline barrier
		if ((barrier.m_flags & BarrierFlags::BARRIER_BEGIN) != 0)
		{
			continue;
		}

		const auto imageFormat = barrier.m_image ? barrier.m_image->getDescription().m_format : Format::UNDEFINED;
		const auto beforeStateInfo = getResourceStateInfo(barrier.m_stateBefore, UtilityVk::translatePipelineStageFlags(barrier.m_stagesBefore), bool(barrier.m_image), imageFormat);
		const auto afterStateInfo = getResourceStateInfo(barrier.m_stateAfter, UtilityVk::translatePipelineStageFlags(barrier.m_stagesAfter), bool(barrier.m_image), imageFormat);

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

	if (m_device->isDynamicRenderingExtensionSupported())
	{
		eastl::fixed_vector<VkRenderingAttachmentInfoKHR, 8> colorAttachmentsVk;
		for (size_t i = 0; i < colorAttachmentCount; ++i)
		{
			const auto &attachment = colorAttachments[i];

			VkRenderingAttachmentInfoKHR attachmentVk{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
			attachmentVk.imageView = static_cast<VkImageView>(attachment.m_imageView->getNativeHandle());
			attachmentVk.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			attachmentVk.resolveMode = VK_RESOLVE_MODE_NONE;
			attachmentVk.resolveImageView; // ignored due to VK_RESOLVE_MODE_NONE
			attachmentVk.resolveImageLayout; // ignored due to VK_RESOLVE_MODE_NONE
			attachmentVk.loadOp = UtilityVk::translate(attachment.m_loadOp);
			attachmentVk.storeOp = UtilityVk::translate(attachment.m_storeOp);
			attachmentVk.clearValue.color = *reinterpret_cast<const VkClearColorValue *>(&attachment.m_clearValue);

			colorAttachmentsVk.push_back(attachmentVk);
		}

		VkRenderingAttachmentInfoKHR depthAttachmentVk{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		VkRenderingAttachmentInfoKHR stencilAttachmentVk{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
		if (depthStencilAttachment)
		{
			depthAttachmentVk.imageView = static_cast<VkImageView>(depthStencilAttachment->m_imageView->getNativeHandle());
			depthAttachmentVk.imageLayout = depthStencilAttachment->m_readOnly ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthAttachmentVk.resolveMode = VK_RESOLVE_MODE_NONE;
			depthAttachmentVk.resolveImageView; // ignored due to VK_RESOLVE_MODE_NONE
			depthAttachmentVk.resolveImageLayout; // ignored due to VK_RESOLVE_MODE_NONE
			depthAttachmentVk.loadOp = UtilityVk::translate(depthStencilAttachment->m_loadOp);
			depthAttachmentVk.storeOp = UtilityVk::translate(depthStencilAttachment->m_storeOp);
			depthAttachmentVk.clearValue.depthStencil = *reinterpret_cast<const VkClearDepthStencilValue *>(&depthStencilAttachment->m_clearValue);

			stencilAttachmentVk = depthAttachmentVk;
			stencilAttachmentVk.loadOp = UtilityVk::translate(depthStencilAttachment->m_stencilLoadOp);
			stencilAttachmentVk.storeOp = UtilityVk::translate(depthStencilAttachment->m_stencilStoreOp);
		}

		VkRenderingInfoKHR renderingInfoVk{ VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
		renderingInfoVk.renderArea = *reinterpret_cast<const VkRect2D *>(&renderArea);
		renderingInfoVk.layerCount = 1;
		renderingInfoVk.viewMask = 0;
		renderingInfoVk.colorAttachmentCount = colorAttachmentCount;
		renderingInfoVk.pColorAttachments = colorAttachmentsVk.data();
		renderingInfoVk.pDepthAttachment = depthStencilAttachment ? &depthAttachmentVk : nullptr;
		renderingInfoVk.pStencilAttachment = depthStencilAttachment ? &stencilAttachmentVk : nullptr;

		vkCmdBeginRenderingKHR(m_commandBuffer, &renderingInfoVk);
	}
	else
	{
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

			framebufferWidth = eastl::min(framebufferWidth, imageDesc.m_width);
			framebufferHeight = eastl::min(framebufferHeight, imageDesc.m_height);

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

			framebufferWidth = eastl::min(framebufferWidth, imageDesc.m_width);
			framebufferHeight = eastl::min(framebufferHeight, imageDesc.m_height);

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
}

void gal::CommandListVk::endRenderPass()
{
	if (m_device->isDynamicRenderingExtensionSupported())
	{
		vkCmdEndRenderingKHR(m_commandBuffer);
	}
	else
	{
		vkCmdEndRenderPass(m_commandBuffer);
	}
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