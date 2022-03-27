#include "RenderGraph.h"
#include "gal/Initializers.h"
#include "ResourceViewRegistry.h"
#include <assert.h>

using namespace gal;

rg::ImageDesc rg::ImageDesc::create(const char *name, gal::Format format, gal::ImageUsageFlags usageFlags, uint32_t width, uint32_t height, uint32_t depth, uint32_t layers, uint32_t levels, gal::ImageType imageType, gal::ClearValue optimizedClearValue, gal::SampleCount samples) noexcept
{
	ImageDesc result;
	result.m_name = name;
	result.m_width = width;
	result.m_height = height;
	result.m_depth = depth;
	result.m_layers = layers;
	result.m_levels = levels;
	result.m_samples = samples;
	result.m_imageType = imageType;
	result.m_format = format;
	result.m_usageFlags = usageFlags;
	result.m_optimizedClearValue = optimizedClearValue;

	return result;
}

rg::BufferDesc rg::BufferDesc::create(const char *name, uint64_t size, gal::BufferUsageFlags usageFlags, bool hostVisible) noexcept
{
	BufferDesc result;
	result.m_name = name;
	result.m_size = size;
	result.m_usageFlags = usageFlags;
	result.m_hostVisible = hostVisible;

	return result;
}

rg::ImageViewDesc rg::ImageViewDesc::create(const char *name, ResourceHandle imageHandle, const gal::ImageSubresourceRange &subresourceRange, gal::ImageViewType viewType, gal::Format format, const gal::ComponentMapping &components) noexcept
{
	ImageViewDesc result;
	result.m_name = name;
	result.m_imageHandle = imageHandle;
	result.m_subresourceRange = subresourceRange;
	result.m_viewType = viewType;
	result.m_format = format;
	result.m_components = components;

	return result;
}

rg::ImageViewDesc rg::ImageViewDesc::createDefault(const char *name, ResourceHandle imageHandle, RenderGraph *graph) noexcept
{
	const auto &resDesc = graph->m_resourceDescriptions[imageHandle - 1];
	
	ImageViewDesc result;
	result.m_name = name;
	result.m_imageHandle = imageHandle;
	result.m_subresourceRange = { 0, resDesc.m_levels, 0, resDesc.m_layers };
	result.m_viewType = static_cast<ImageViewType>(resDesc.m_imageType);
	result.m_format = resDesc.m_format;
	result.m_components = {};

	return result;
}

rg::BufferViewDesc rg::BufferViewDesc::create(const char *name, ResourceHandle bufferHandle, uint64_t range, uint64_t offset, uint32_t structureByteStride, gal::Format format) noexcept
{
	BufferViewDesc result;
	result.m_name = name;
	result.m_bufferHandle = bufferHandle;
	result.m_offset = offset;
	result.m_range = range;
	result.m_structureByteStride = structureByteStride;
	result.m_format = format;

	return result;
}

rg::BufferViewDesc rg::BufferViewDesc::createDefault(const char *name, ResourceHandle bufferHandle, RenderGraph *graph) noexcept
{
	const auto &resDesc = graph->m_resourceDescriptions[bufferHandle - 1];

	BufferViewDesc result;
	result.m_name = name;
	result.m_bufferHandle = bufferHandle;
	result.m_offset = 0;
	result.m_range = resDesc.m_size;
	result.m_structureByteStride = 0;
	result.m_format = Format::UNDEFINED;

	return result;
}





rg::RenderGraph::RenderGraph(gal::GraphicsDevice *device, gal::Semaphore **semaphores, uint64_t *semaphoreValues, ResourceViewRegistry *resourceViewRegistry) noexcept
	:m_device(device),
	m_resourceViewRegistry(resourceViewRegistry)
{
	m_queues[0] = m_device->getGraphicsQueue();
	m_queues[1] = m_device->getComputeQueue();
	m_queues[2] = m_device->getTransferQueue();

	for (size_t i = 0; i < 3; ++i)
	{
		m_semaphores[i] = semaphores[i];
		m_semaphoreValues[i] = semaphoreValues + i;
	}

	for (size_t i = 0; i < k_numFrames; ++i)
	{
		m_frameResources[i].m_commandListFramePool.init(m_device);
	}
}

rg::RenderGraph::~RenderGraph() noexcept
{
	for (size_t i = 0; i < k_numFrames; ++i)
	{
		nextFrame();
	}
}

void rg::RenderGraph::nextFrame() noexcept
{
	++m_frame;

	// reset old frame resources
	{
		auto &frameResources = m_frameResources[m_frame % k_numFrames];

		// wait on semaphores for queue completion
		for (size_t i = 0; i < 3; ++i)
		{
			m_semaphores[i]->wait(frameResources.m_finalWaitValues[i]);
		}

		// destroy internal resources
		for (auto &res : frameResources.m_resources)
		{
			if (res.m_external)
			{
				continue;
			}
			if (res.m_image)
			{
				m_device->destroyImage(res.m_image);
			}
			else if (res.m_buffer)
			{
				m_device->destroyBuffer(res.m_buffer);
			}
		}
		frameResources.m_resources.clear();

		// destroy views
		for (auto &view : frameResources.m_resourceViews)
		{
			if (view.m_imageView)
			{
				m_device->destroyImageView(view.m_imageView);
			}
			else if (view.m_bufferView)
			{
				m_device->destroyBufferView(view.m_bufferView);
			}
		}
		frameResources.m_resourceViews.clear();

		// reset command lists
		frameResources.m_commandListFramePool.reset();
	}
	
	// clear old vectors
	m_resourceDescriptions.clear();
	m_viewDescriptions.clear();
	m_culledResources.clear();
	m_subresourceUsages.clear();
	m_passData.clear();
	m_recordBatches.clear();
	m_externalReleaseBarriers[0].clear();
	m_externalReleaseBarriers[1].clear();
	m_externalReleaseBarriers[2].clear();
}

rg::ResourceViewHandle rg::RenderGraph::createImageView(const ImageViewDesc &viewDesc) noexcept
{
	auto &resDesc = m_resourceDescriptions[viewDesc.m_imageHandle - 1];
	assert(resDesc.m_image);
	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = viewDesc.m_imageHandle;
	desc.m_viewType = viewDesc.m_viewType;
	desc.m_format = viewDesc.m_format == Format::UNDEFINED ? resDesc.m_format : viewDesc.m_format;
	desc.m_components = viewDesc.m_components;
	desc.m_subresourceRange = viewDesc.m_subresourceRange;
	desc.m_image = true;

	m_viewDescriptions.push_back(desc);

	if (viewDesc.m_format != resDesc.m_format)
	{
		resDesc.m_imageFlags |= ImageCreateFlags::MUTABLE_FORMAT_BIT;
	}
	if (viewDesc.m_viewType == ImageViewType::CUBE || viewDesc.m_viewType == ImageViewType::CUBE_ARRAY)
	{
		resDesc.m_imageFlags |= ImageCreateFlags::CUBE_COMPATIBLE_BIT;
	}
	else if (resDesc.m_imageType == ImageType::_3D && viewDesc.m_viewType == ImageViewType::_2D_ARRAY)
	{
		resDesc.m_imageFlags |= ImageCreateFlags::_2D_ARRAY_COMPATIBLE_BIT;
	}

	return ResourceViewHandle(m_viewDescriptions.size());
}

rg::ResourceViewHandle rg::RenderGraph::createBufferView(const BufferViewDesc &viewDesc) noexcept
{
	assert(!m_resourceDescriptions[viewDesc.m_bufferHandle - 1].m_image);

	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = viewDesc.m_bufferHandle;
	desc.m_format = viewDesc.m_format;
	desc.m_offset = viewDesc.m_offset;
	desc.m_range = viewDesc.m_range;
	desc.m_structureByteStride = viewDesc.m_structureByteStride;

	m_viewDescriptions.push_back(desc);

	return ResourceViewHandle(m_viewDescriptions.size());
}

rg::ResourceHandle rg::RenderGraph::createImage(const ImageDesc &imageDesc) noexcept
{
	ResourceDescription resDesc{};
	resDesc.m_name = imageDesc.m_name;
	resDesc.m_usageFlags = static_cast<uint32_t>(imageDesc.m_usageFlags);
	resDesc.m_width = imageDesc.m_width;
	resDesc.m_height = imageDesc.m_height;
	resDesc.m_depth = imageDesc.m_depth;
	resDesc.m_layers = imageDesc.m_layers;
	resDesc.m_levels = imageDesc.m_levels;
	resDesc.m_samples = imageDesc.m_samples;
	resDesc.m_imageType = imageDesc.m_imageType;
	resDesc.m_format = imageDesc.m_format;
	resDesc.m_optimizedClearValue = imageDesc.m_optimizedClearValue;
	resDesc.m_subresourceCount = resDesc.m_layers * resDesc.m_levels;
	resDesc.m_subresourceUsageInfoOffset = static_cast<uint32_t>(m_subresourceUsages.size());
	resDesc.m_concurrent = false;
	resDesc.m_image = true;

	assert(resDesc.m_width && resDesc.m_height && resDesc.m_layers && resDesc.m_levels);

	m_resourceDescriptions.push_back(resDesc);
	for (size_t i = 0; i < resDesc.m_subresourceCount; ++i)
	{
		m_subresourceUsages.push_back({});
	}

	return ResourceHandle(m_resourceDescriptions.size());
}

rg::ResourceHandle rg::RenderGraph::createBuffer(const BufferDesc &bufferDesc) noexcept
{
	ResourceDescription resDesc{};
	resDesc.m_name = bufferDesc.m_name;
	resDesc.m_usageFlags = static_cast<uint32_t>(bufferDesc.m_usageFlags);
	resDesc.m_offset = 0;
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;
	resDesc.m_subresourceUsageInfoOffset = static_cast<uint32_t>(m_subresourceUsages.size());
	resDesc.m_concurrent = true;
	resDesc.m_hostVisible = bufferDesc.m_hostVisible;

	assert(resDesc.m_size);

	m_resourceDescriptions.push_back(resDesc);
	m_subresourceUsages.push_back({});

	return ResourceHandle(m_resourceDescriptions.size());
}

rg::ResourceHandle rg::RenderGraph::importImage(gal::Image *image, const char *name, ResourceStateData *resourceStateData) noexcept
{
	const auto &imageDesc = image->getDescription();

	ResourceDescription resDesc{};
	resDesc.m_name = name;
	resDesc.m_usageFlags = static_cast<uint32_t>(imageDesc.m_usageFlags);
	resDesc.m_width = imageDesc.m_width;
	resDesc.m_height = imageDesc.m_height;
	resDesc.m_depth = imageDesc.m_depth;
	resDesc.m_layers = imageDesc.m_layers;
	resDesc.m_levels = imageDesc.m_levels;
	resDesc.m_samples = imageDesc.m_samples;
	resDesc.m_imageType = imageDesc.m_imageType;
	resDesc.m_format = imageDesc.m_format;
	resDesc.m_optimizedClearValue = imageDesc.m_optimizedClearValue;
	resDesc.m_subresourceCount = resDesc.m_layers * resDesc.m_levels;
	resDesc.m_subresourceUsageInfoOffset = static_cast<uint32_t>(m_subresourceUsages.size());
	resDesc.m_externalStateData = resourceStateData;
	resDesc.m_concurrent = false;
	resDesc.m_external = true;
	resDesc.m_image = true;

	assert(resDesc.m_width && resDesc.m_height && resDesc.m_layers && resDesc.m_levels);

	m_resourceDescriptions.push_back(resDesc);
	for (size_t i = 0; i < resDesc.m_subresourceCount; ++i)
	{
		m_subresourceUsages.push_back({});
	}

	auto &frameResources = m_frameResources[m_frame % k_numFrames];
	frameResources.m_resources.resize(m_resourceDescriptions.size());
	frameResources.m_resources.back().m_image = image;
	frameResources.m_resources.back().m_external = true;

	m_device->setDebugObjectName(ObjectType::IMAGE, image, name);

	return ResourceHandle(m_resourceDescriptions.size());
}

rg::ResourceHandle rg::RenderGraph::importBuffer(gal::Buffer *buffer, const char *name, ResourceStateData *resourceStateData) noexcept
{
	const auto &bufferDesc = buffer->getDescription();

	ResourceDescription resDesc{};
	resDesc.m_name = name;
	resDesc.m_usageFlags = static_cast<uint32_t>(bufferDesc.m_usageFlags);
	resDesc.m_offset = 0; // TODO
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;
	resDesc.m_subresourceUsageInfoOffset = static_cast<uint32_t>(m_subresourceUsages.size());
	resDesc.m_externalStateData = resourceStateData;
	resDesc.m_concurrent = true;
	resDesc.m_external = true;

	assert(resDesc.m_size);

	m_resourceDescriptions.push_back(resDesc);
	m_subresourceUsages.push_back({});

	auto &frameResources = m_frameResources[m_frame % k_numFrames];
	frameResources.m_resources.resize(m_resourceDescriptions.size());
	frameResources.m_resources.back().m_buffer = buffer;
	frameResources.m_resources.back().m_external = true;

	m_device->setDebugObjectName(ObjectType::BUFFER, buffer, name);

	return ResourceHandle(m_resourceDescriptions.size());
}

void rg::RenderGraph::addPass(const char *name, QueueType queueType, size_t usageCount, const ResourceUsageDesc *usageDescs, const RecordFunc &recordFunc) noexcept
{
#ifdef _DEBUG
	for (uint32_t i = 0; i < usageCount; ++i)
	{
		// catches most false api usages
		assert(usageDescs[i].m_viewHandle);
		// not a very good test, but if the handle has a higher value than the number of views, something must be off
		assert(size_t(usageDescs[i].m_viewHandle) <= m_viewDescriptions.size());
	}
#endif // _DEBUG

	PassData passData{};
	passData.m_recordFunc = recordFunc;
	passData.m_name = name;
	passData.m_queue = m_queues[static_cast<size_t>(queueType)];

	const uint16_t passIndex = static_cast<uint16_t>(m_passData.size());
	m_passData.push_back(passData);

	// set subresource usages in this pass
	for (size_t i = 0; i < usageCount; ++i)
	{
		const auto &usage = usageDescs[i];
		assert(usage.m_viewHandle != 0);
		const auto &viewDesc = m_viewDescriptions[usage.m_viewHandle - 1];
		const SubResourceUsage resUsage{ passIndex, usage.m_stateAndStage, usage.m_finalStateAndStage.m_resourceState == ResourceState::UNDEFINED ? usage.m_stateAndStage : usage.m_finalStateAndStage };

		const size_t resIndex = (size_t)viewDesc.m_resourceHandle - 1;
		const auto &resDesc = m_resourceDescriptions[resIndex];

		if (resDesc.m_image)
		{
			const uint32_t baseLayer = viewDesc.m_subresourceRange.m_baseArrayLayer;
			const uint32_t layerCount = viewDesc.m_subresourceRange.m_layerCount;
			const uint32_t baseLevel = viewDesc.m_subresourceRange.m_baseMipLevel;
			const uint32_t levelCount = viewDesc.m_subresourceRange.m_levelCount;
			for (uint32_t layer = 0; layer < layerCount; ++layer)
			{
				for (uint32_t level = 0; level < levelCount; ++level)
				{
					const uint32_t index = (layer + baseLayer) * resDesc.m_levels + (level + baseLevel) + resDesc.m_subresourceUsageInfoOffset;
					m_subresourceUsages[index].push_back(resUsage);
				}
			}
		}
		else
		{
			m_subresourceUsages[resDesc.m_subresourceUsageInfoOffset].push_back(resUsage);
		}
	}
}

void rg::RenderGraph::execute() noexcept
{
	createResources();
	createSynchronization();
	m_resourceViewRegistry->flushChanges();
	recordAndSubmit();
}

void rg::RenderGraph::createResources() noexcept
{
	auto &frameResources = m_frameResources[m_frame % k_numFrames];
	frameResources.m_resources.resize(m_resourceDescriptions.size());
	m_culledResources.resize(m_resourceDescriptions.size());
	frameResources.m_resourceViews.resize(m_viewDescriptions.size());

	// create resources
	const size_t resourceCount = m_resourceDescriptions.size();
	for (size_t resourceIdx = 0; resourceIdx < resourceCount; ++resourceIdx)
	{
		const auto &resDesc = m_resourceDescriptions[resourceIdx];

		if (resDesc.m_external)
		{
			continue;
		}

		// check if resource is used at all and also find usage flags
		bool isReferenced = false;
		uint32_t usageFlags = resDesc.m_usageFlags;

		const size_t subresourceCount = resDesc.m_subresourceCount;
		for (size_t subresourceIdx = 0; subresourceIdx < subresourceCount; ++subresourceIdx)
		{
			const size_t subresourceUsageIdx = subresourceIdx + resDesc.m_subresourceUsageInfoOffset;

			// check if subresource is used at all
			isReferenced = isReferenced || !m_subresourceUsages[subresourceUsageIdx].empty();

			// get usage flags
			for (const auto &usage : m_subresourceUsages[subresourceUsageIdx])
			{
				usageFlags |= Initializers::getUsageFlags(usage.m_initialResourceState.m_resourceState, resDesc.m_image);
				usageFlags |= Initializers::getUsageFlags(usage.m_finalResourceState.m_resourceState, resDesc.m_image);
			}
		}

		// resource has no references -> no need to create an actual resource
		if (!isReferenced)
		{
			m_culledResources[resourceIdx] = true;
			continue;
		}

		// is resource image or buffer?
		if (resDesc.m_image)
		{
			// create image
			ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_width = resDesc.m_width;
			imageCreateInfo.m_height = resDesc.m_height;
			imageCreateInfo.m_depth = resDesc.m_depth;
			imageCreateInfo.m_levels = resDesc.m_levels;
			imageCreateInfo.m_layers = resDesc.m_layers;
			imageCreateInfo.m_samples = resDesc.m_samples;
			imageCreateInfo.m_imageType = resDesc.m_imageType;
			imageCreateInfo.m_format = resDesc.m_format;
			imageCreateInfo.m_createFlags = resDesc.m_imageFlags;
			imageCreateInfo.m_usageFlags = static_cast<ImageUsageFlags>(usageFlags);
			imageCreateInfo.m_optimizedClearValue = resDesc.m_optimizedClearValue;

			m_device->createImage(imageCreateInfo, MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &frameResources.m_resources[resourceIdx].m_image);
			m_device->setDebugObjectName(ObjectType::IMAGE, frameResources.m_resources[resourceIdx].m_image, resDesc.m_name);
		}
		else
		{
			// create buffer
			BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = resDesc.m_size;
			bufferCreateInfo.m_createFlags = {};
			bufferCreateInfo.m_usageFlags = static_cast<BufferUsageFlags>(usageFlags);

			auto requiredFlags = resDesc.m_hostVisible ? (MemoryPropertyFlags::HOST_VISIBLE_BIT | MemoryPropertyFlags::HOST_COHERENT_BIT) : MemoryPropertyFlags::DEVICE_LOCAL_BIT;
			auto preferredFlags = resDesc.m_hostVisible ? MemoryPropertyFlags::DEVICE_LOCAL_BIT : MemoryPropertyFlags{};

			m_device->createBuffer(bufferCreateInfo, requiredFlags, preferredFlags, false, &frameResources.m_resources[resourceIdx].m_buffer);
			m_device->setDebugObjectName(ObjectType::BUFFER, frameResources.m_resources[resourceIdx].m_buffer, resDesc.m_name);
		}
	}

	// create views
	const size_t viewCount = m_viewDescriptions.size();
	for (uint32_t viewIndex = 0; viewIndex < m_viewDescriptions.size(); ++viewIndex)
	{
		const auto &viewDesc = m_viewDescriptions[viewIndex];

		if (m_culledResources[viewDesc.m_resourceHandle - 1])
		{
			continue;
		}

		auto &viewData = frameResources.m_resourceViews[viewIndex];
		viewData = {};

		if (viewDesc.m_image)
		{
			ImageViewCreateInfo viewCreateInfo{};
			viewCreateInfo.m_image = frameResources.m_resources[viewDesc.m_resourceHandle - 1].m_image;
			viewCreateInfo.m_viewType = viewDesc.m_viewType;
			viewCreateInfo.m_format = viewDesc.m_format;
			viewCreateInfo.m_components = viewDesc.m_components;
			viewCreateInfo.m_baseMipLevel = viewDesc.m_subresourceRange.m_baseMipLevel;
			viewCreateInfo.m_levelCount = viewDesc.m_subresourceRange.m_levelCount;
			viewCreateInfo.m_baseArrayLayer = viewDesc.m_subresourceRange.m_baseArrayLayer;
			viewCreateInfo.m_layerCount = viewDesc.m_subresourceRange.m_layerCount;

			m_device->createImageView(viewCreateInfo, &viewData.m_imageView);
			m_device->setDebugObjectName(ObjectType::IMAGE_VIEW, viewData.m_imageView, viewDesc.m_name);

			const auto usageFlags = viewCreateInfo.m_image->getDescription().m_usageFlags;

			if ((usageFlags & ImageUsageFlags::TEXTURE_BIT) != 0)
			{
				viewData.m_textureHandle = m_resourceViewRegistry->createTextureViewHandle(viewData.m_imageView, true);
			}
			if ((usageFlags & ImageUsageFlags::RW_TEXTURE_BIT) != 0)
			{
				viewData.m_rwTextureHandle = m_resourceViewRegistry->createRWTextureViewHandle(viewData.m_imageView, true);
			}
		}
		else if (viewDesc.m_format != Format::UNDEFINED)
		{
			BufferViewCreateInfo viewCreateInfo{};
			viewCreateInfo.m_buffer = frameResources.m_resources[viewDesc.m_resourceHandle - 1].m_buffer;
			viewCreateInfo.m_format = viewDesc.m_format;
			viewCreateInfo.m_offset = viewDesc.m_offset;
			viewCreateInfo.m_range = viewDesc.m_range;

			m_device->createBufferView(viewCreateInfo, &viewData.m_bufferView);
			m_device->setDebugObjectName(ObjectType::BUFFER_VIEW, viewData.m_bufferView, viewDesc.m_name);
		}

		if (!viewDesc.m_image)
		{
			auto buffer = frameResources.m_resources[viewDesc.m_resourceHandle - 1].m_buffer;
			const auto usageFlags = buffer->getDescription().m_usageFlags;
			const auto &resDesc = m_resourceDescriptions[viewDesc.m_resourceHandle - 1];
			DescriptorBufferInfo bufferInfo = { buffer, resDesc.m_offset + viewDesc.m_offset, viewDesc.m_range, viewDesc.m_structureByteStride };

			if ((usageFlags & BufferUsageFlags::TYPED_BUFFER_BIT) != 0 && viewData.m_bufferView)
			{
				viewData.m_typedBufferHandle = m_resourceViewRegistry->createTypedBufferViewHandle(viewData.m_bufferView, true);
			}
			if ((usageFlags & BufferUsageFlags::RW_TYPED_BUFFER_BIT) != 0 && viewData.m_bufferView)
			{
				viewData.m_rwTypedBufferHandle = m_resourceViewRegistry->createRWTypedBufferViewHandle(viewData.m_bufferView, true);
			}
			if ((usageFlags & BufferUsageFlags::BYTE_BUFFER_BIT) != 0)
			{
				viewData.m_byteBufferHandle = m_resourceViewRegistry->createByteBufferViewHandle(bufferInfo, true);
			}
			if ((usageFlags & BufferUsageFlags::RW_BYTE_BUFFER_BIT) != 0)
			{
				viewData.m_rwByteBufferHandle = m_resourceViewRegistry->createRWByteBufferViewHandle(bufferInfo, true);
			}
			if (viewDesc.m_structureByteStride != 0 && (usageFlags & BufferUsageFlags::STRUCTURED_BUFFER_BIT) != 0)
			{
				viewData.m_structuredBufferHandle = m_resourceViewRegistry->createStructuredBufferViewHandle(bufferInfo, true);
			}
			if (viewDesc.m_structureByteStride != 0 && (usageFlags & BufferUsageFlags::RW_STRUCTURED_BUFFER_BIT) != 0)
			{
				viewData.m_rwStructuredBufferHandle = m_resourceViewRegistry->createRWStructuredBufferViewHandle(bufferInfo, true);
			}
		}
	}
}

void rg::RenderGraph::createSynchronization() noexcept
{
	struct SemaphoreDependencyInfo
	{
		PipelineStageFlags m_waitDstStageMasks[3] = {};
		uint64_t m_waitValues[3] = {};
	};

	struct UsageInfo
	{
		uint16_t m_passHandle;
		Queue *m_queue;
		ResourceStateAndStage m_stateAndStage;
	};

	auto &frameResources = m_frameResources[m_frame % k_numFrames];

	eastl::vector<SemaphoreDependencyInfo> semaphoreDependencies(m_passData.size());

	// for each resource...
	const size_t resourceCount = m_resourceDescriptions.size();
	for (size_t resourceIdx = 0; resourceIdx < resourceCount; ++resourceIdx)
	{
		// skip culled resources
		if (m_culledResources[resourceIdx])
		{
			continue;
		}

		const auto &resDesc = m_resourceDescriptions[resourceIdx];

		// for each subresource...
		const size_t subresourceCount = resDesc.m_subresourceCount;
		for (size_t subresourceIdx = 0; subresourceIdx < subresourceCount; ++subresourceIdx)
		{
			const size_t subresourceUsageIdx = subresourceIdx + resDesc.m_subresourceUsageInfoOffset;

			// check if subresource is used at all
			if (m_subresourceUsages[subresourceUsageIdx].empty())
			{
				continue;
			}

			UsageInfo prevUsageInfo{};
			prevUsageInfo.m_queue = m_passData[m_subresourceUsages[subresourceUsageIdx][0].m_passHandle].m_queue;

			// if the resource is external, change prevUsageInfo accordingly and update external info values
			if (resDesc.m_external)
			{
				const auto &extInfo = resDesc.m_externalStateData;
				prevUsageInfo.m_queue = extInfo && extInfo[subresourceIdx].m_queue ? extInfo[subresourceIdx].m_queue : prevUsageInfo.m_queue;
				prevUsageInfo.m_stateAndStage = extInfo ? extInfo[subresourceIdx].m_stateAndStage : prevUsageInfo.m_stateAndStage;
				prevUsageInfo.m_passHandle = prevUsageInfo.m_queue == m_queues[0] ? 0 : prevUsageInfo.m_queue == m_queues[1] ? 1 : 2;

				// update external info values
				if (extInfo)
				{
					const auto &lastUsage = m_subresourceUsages[subresourceUsageIdx].back();
					extInfo[subresourceIdx].m_queue = m_passData[lastUsage.m_passHandle].m_queue;
					extInfo[subresourceIdx].m_stateAndStage = lastUsage.m_finalResourceState;
				}
			}

			// for each usage...
			const size_t usageCount = m_subresourceUsages[subresourceUsageIdx].size();
			for (size_t usageIdx = 0; usageIdx < usageCount;)
			{
				const auto &subresUsage = m_subresourceUsages[subresourceUsageIdx][usageIdx];
				auto &passData = m_passData[subresUsage.m_passHandle];

				UsageInfo curUsageInfo{ subresUsage.m_passHandle, passData.m_queue, subresUsage.m_initialResourceState };

				// look ahead and try to combine READ_* states
				size_t nextUsageIdx = usageIdx + 1;
				ResourceState combinableImageReadStates = ResourceState::READ_RESOURCE | ResourceState::READ_DEPTH_STENCIL;
				ResourceState combinableBufferReadStates = ResourceState::READ_RESOURCE | ResourceState::READ_CONSTANT_BUFFER | ResourceState::READ_VERTEX_BUFFER | ResourceState::READ_INDEX_BUFFER | ResourceState::READ_INDIRECT_BUFFER | ResourceState::READ_TRANSFER;

				const bool hasNoCustomFinalState = curUsageInfo.m_stateAndStage.m_resourceState == subresUsage.m_finalResourceState.m_resourceState &&
					curUsageInfo.m_stateAndStage.m_stageMask == subresUsage.m_finalResourceState.m_stageMask;

				// is the current state even READ combinable? custom final state breaks this too
				
				if (hasNoCustomFinalState &&
					(resDesc.m_image && (curUsageInfo.m_stateAndStage.m_resourceState & combinableImageReadStates) != 0 ||
					!resDesc.m_image && (curUsageInfo.m_stateAndStage.m_resourceState & combinableBufferReadStates) != 0))
				{
					for (; nextUsageIdx < usageCount; ++nextUsageIdx)
					{
						const auto &nextSubresUsage = m_subresourceUsages[subresourceUsageIdx][nextUsageIdx];
						auto &nextPassData = m_passData[nextSubresUsage.m_passHandle];
						UsageInfo nextUsageInfo{ nextSubresUsage.m_passHandle, nextPassData.m_queue, nextSubresUsage.m_initialResourceState };

						const bool sameQueue = nextPassData.m_queue == passData.m_queue;

						const bool noCustomFinalState = nextUsageInfo.m_stateAndStage.m_resourceState == nextSubresUsage.m_finalResourceState.m_resourceState &&
							nextUsageInfo.m_stateAndStage.m_stageMask == nextSubresUsage.m_finalResourceState.m_stageMask;

						const bool combinable = (resDesc.m_image && (nextUsageInfo.m_stateAndStage.m_resourceState & combinableImageReadStates) != 0 ||
							!resDesc.m_image && (nextUsageInfo.m_stateAndStage.m_resourceState & combinableBufferReadStates) != 0);

						if (!sameQueue || !combinable || !noCustomFinalState)
						{
							break;
						}

						curUsageInfo.m_stateAndStage.m_stageMask |= nextUsageInfo.m_stateAndStage.m_stageMask;
						curUsageInfo.m_stateAndStage.m_resourceState |= nextUsageInfo.m_stateAndStage.m_resourceState;
					}
				}

				Barrier barrier{};
				barrier.m_image = resDesc.m_image ? frameResources.m_resources[resourceIdx].m_image : nullptr;
				barrier.m_buffer = !resDesc.m_image ? frameResources.m_resources[resourceIdx].m_buffer : nullptr;
				barrier.m_stagesBefore = prevUsageInfo.m_stateAndStage.m_stageMask;
				barrier.m_stagesAfter = curUsageInfo.m_stateAndStage.m_stageMask;
				barrier.m_stateBefore = prevUsageInfo.m_stateAndStage.m_resourceState;
				barrier.m_stateAfter = curUsageInfo.m_stateAndStage.m_resourceState;
				barrier.m_srcQueue = prevUsageInfo.m_queue;
				barrier.m_dstQueue = curUsageInfo.m_queue;
				barrier.m_imageSubresourceRange = { static_cast<uint32_t>(subresourceIdx) % resDesc.m_levels, 1, static_cast<uint32_t>(subresourceIdx) / resDesc.m_levels, 1 };
				if (barrier.m_srcQueue != barrier.m_dstQueue && !resDesc.m_concurrent)
				{
					barrier.m_flags |= BarrierFlags::QUEUE_OWNERSHIP_AQUIRE;
				}
				if (usageIdx == 0)
				{
					barrier.m_flags |= BarrierFlags::FIRST_ACCESS_IN_SUBMISSION;
				}

				passData.m_beforeBarriers.push_back(barrier);

				const size_t prevQueueIdx = prevUsageInfo.m_queue == m_queues[0] ? 0 : prevUsageInfo.m_queue == m_queues[1] ? 1 : 2;

				// we just acquired ownership of the resource -> add a release barrier on the previous queue
				if ((barrier.m_flags & BarrierFlags::QUEUE_OWNERSHIP_AQUIRE) != 0)
				{
					barrier.m_flags ^= BarrierFlags::QUEUE_OWNERSHIP_AQUIRE;
					barrier.m_flags |= BarrierFlags::QUEUE_OWNERSHIP_RELEASE;

					semaphoreDependencies[curUsageInfo.m_passHandle].m_waitDstStageMasks[prevQueueIdx] |= curUsageInfo.m_stateAndStage.m_stageMask;
					auto &waitValue = semaphoreDependencies[curUsageInfo.m_passHandle].m_waitValues[prevQueueIdx];

					// external dependency
					if (usageIdx == 0)
					{
						m_externalReleaseBarriers[prevUsageInfo.m_passHandle].push_back(barrier);
						waitValue = eastl::max<uint64_t>(*m_semaphoreValues[prevQueueIdx] + 1, waitValue);
					}
					else
					{
						auto &prevPassRecordInfo = m_passData[prevUsageInfo.m_passHandle];
						prevPassRecordInfo.m_afterBarriers.push_back(barrier);
						waitValue = eastl::max<uint64_t>(*m_semaphoreValues[prevQueueIdx] + 1 + prevPassRecordInfo.m_signalValue, waitValue);
					}
				}

				// update prevUsageInfo
				prevUsageInfo = curUsageInfo;
				if (!hasNoCustomFinalState)
				{
					prevUsageInfo.m_stateAndStage = subresUsage.m_finalResourceState;
				}

				usageIdx = nextUsageIdx;
			}
		}
	}

	// create batches
	Queue *prevQueue = nullptr;
	bool startNewBatch = true;
	for (size_t i = 0; i < m_passData.size(); ++i)
	{
		const auto passHandle = i;
		const auto &semaphoreDependency = semaphoreDependencies[passHandle];
		Queue *curQueue = m_passData[passHandle].m_queue;

		// if the previous pass needs to signal, startNewBatch is already true
		// if the queue type changed, we need to start a new batch
		// if this pass needs to wait, we also need a need batch
		startNewBatch = startNewBatch
			|| prevQueue != curQueue
			|| semaphoreDependency.m_waitDstStageMasks[0] != 0
			|| semaphoreDependency.m_waitDstStageMasks[1] != 0
			|| semaphoreDependency.m_waitDstStageMasks[2] != 0;
		if (startNewBatch)
		{
			startNewBatch = false;
			m_recordBatches.push_back({});
			auto &batch = m_recordBatches.back();
			batch.m_queue = curQueue;
			batch.m_passIndexOffset = static_cast<uint16_t>(i);
			for (size_t j = 0; j < 3; ++j)
			{
				batch.m_waitDstStageMasks[j] = semaphoreDependency.m_waitDstStageMasks[j];
				batch.m_waitValues[j] = semaphoreDependency.m_waitValues[j];
			}
		}

		// some other passes needs to wait on this one or this is the last pass -> end batch after this pass
		if (!m_passData[passHandle].m_afterBarriers.empty() || i == m_passData.size() - 1)
		{
			startNewBatch = true;
		}

		// update signal value to current last pass in batch
		auto &batch = m_recordBatches.back();
		const size_t queueIdx = curQueue == m_queues[0] ? 0 : curQueue == m_queues[1] ? 1 : 2;
		batch.m_signalValue = eastl::max<uint64_t>(*m_semaphoreValues[queueIdx] + 1 + m_passData[passHandle].m_signalValue, batch.m_signalValue);

		prevQueue = curQueue;
		++batch.m_passIndexCount;
	}
}

void rg::RenderGraph::recordAndSubmit() noexcept
{
	auto &frameResources = m_frameResources[m_frame % k_numFrames];
	frameResources.m_finalWaitValues[0] = *m_semaphoreValues[0];
	frameResources.m_finalWaitValues[1] = *m_semaphoreValues[1];
	frameResources.m_finalWaitValues[2] = *m_semaphoreValues[2];

	// issue release queue ownership transfer barriers for external resources on the wrong queue
	for (size_t i = 0; i < 3; ++i)
	{
		const char *releasePassNames[]
		{
			"release barriers for external resources (graphics queue)",
			"release barriers for external resources (compute queue)",
			"release barriers for external resources (transfer queue)",
		};

		if (!m_externalReleaseBarriers[i].empty())
		{
			CommandList *cmdList = frameResources.m_commandListFramePool.acquire(m_queues[i]);

			cmdList->begin();

			cmdList->insertDebugLabel(releasePassNames[i]);
			cmdList->barrier(static_cast<uint32_t>(m_externalReleaseBarriers[i].size()), m_externalReleaseBarriers[i].data());

			cmdList->end();

			// submit to queue
			{
				uint64_t waitValue = *m_semaphoreValues[i];
				uint64_t signalValue = waitValue + 1;
				auto waitDstStageMask = PipelineStageFlags::TOP_OF_PIPE_BIT;

				SubmitInfo submitInfo{};
				submitInfo.m_waitSemaphoreCount = 1;
				submitInfo.m_waitSemaphores = &m_semaphores[i];
				submitInfo.m_waitValues = &waitValue;
				submitInfo.m_waitDstStageMask = &waitDstStageMask;
				submitInfo.m_commandListCount = 1;
				submitInfo.m_commandLists = &cmdList;
				submitInfo.m_signalSemaphoreCount = 1;
				submitInfo.m_signalSemaphores = &m_semaphores[i];
				submitInfo.m_signalValues = &signalValue;

				m_queues[i]->submit(1, &submitInfo);
				frameResources.m_finalWaitValues[i] = eastl::max<uint64_t>(frameResources.m_finalWaitValues[i], signalValue);
			}
		}
	}

	for (const auto &batch : m_recordBatches)
	{
		// allocate command list
		CommandList *cmdList = frameResources.m_commandListFramePool.acquire(batch.m_queue);

		cmdList->begin();

		// record passes
		for (size_t i = 0; i < batch.m_passIndexCount; ++i)
		{
			const auto &passData = m_passData[i + batch.m_passIndexOffset];

			ScopedLabel debugLabel(cmdList, passData.m_name);

			// before-barriers
			if (!passData.m_beforeBarriers.empty())
			{
				cmdList->barrier(static_cast<uint32_t>(passData.m_beforeBarriers.size()), passData.m_beforeBarriers.data());
			}

			// record commands
			passData.m_recordFunc(cmdList, Registry(this));

			// after-barriers
			if (!passData.m_afterBarriers.empty())
			{
				cmdList->barrier(static_cast<uint32_t>(passData.m_afterBarriers.size()), passData.m_afterBarriers.data());
			}
		}

		cmdList->end();

		// submit to queue
		{
			gal::Semaphore *waitSemaphores[3];
			PipelineStageFlags waitDstStageMasks[3];
			uint64_t waitValues[3];
			uint32_t waitCount = 0;
			for (size_t i = 0; i < 3; ++i)
			{
				if (batch.m_waitDstStageMasks[i] != 0)
				{
					waitSemaphores[waitCount] = m_semaphores[i];
					waitDstStageMasks[waitCount] = batch.m_waitDstStageMasks[i];
					waitValues[waitCount] = batch.m_waitValues[i];
					++waitCount;
				}
			}

			const size_t queueIdx = batch.m_queue == m_queues[0] ? 0 : batch.m_queue == m_queues[1] ? 1 : 2;

			SubmitInfo submitInfo{};
			submitInfo.m_waitSemaphoreCount = waitCount;
			submitInfo.m_waitSemaphores = waitSemaphores;
			submitInfo.m_waitValues = waitValues;
			submitInfo.m_waitDstStageMask = waitDstStageMasks;
			submitInfo.m_commandListCount = 1;
			submitInfo.m_commandLists = &cmdList;
			submitInfo.m_signalSemaphoreCount = 1;
			submitInfo.m_signalSemaphores = &m_semaphores[queueIdx];
			submitInfo.m_signalValues = &batch.m_signalValue;

			batch.m_queue->submit(1, &submitInfo);

			frameResources.m_finalWaitValues[queueIdx] = eastl::max<uint64_t>(frameResources.m_finalWaitValues[queueIdx], batch.m_signalValue);
		}
	}

	// update global semaphore values
	*m_semaphoreValues[0] = frameResources.m_finalWaitValues[0];
	*m_semaphoreValues[1] = frameResources.m_finalWaitValues[1];
	*m_semaphoreValues[2] = frameResources.m_finalWaitValues[2];
}

rg::Registry::Registry(RenderGraph *renderGraph) noexcept
	:m_graph(renderGraph)
{
}

uint32_t rg::Registry::getBindlessHandle(ResourceViewHandle handle, gal::DescriptorType type) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];
	switch (type)
	{
	case gal::DescriptorType::TEXTURE:
		return frameResources.m_resourceViews[handle - 1].m_textureHandle;
	case gal::DescriptorType::RW_TEXTURE:
		return frameResources.m_resourceViews[handle - 1].m_rwTextureHandle;
	case gal::DescriptorType::TYPED_BUFFER:
		return frameResources.m_resourceViews[handle - 1].m_typedBufferHandle;
	case gal::DescriptorType::RW_TYPED_BUFFER:
		return frameResources.m_resourceViews[handle - 1].m_rwTypedBufferHandle;
	case gal::DescriptorType::BYTE_BUFFER:
		return frameResources.m_resourceViews[handle - 1].m_byteBufferHandle;
	case gal::DescriptorType::RW_BYTE_BUFFER:
		return frameResources.m_resourceViews[handle - 1].m_rwByteBufferHandle;
	case gal::DescriptorType::STRUCTURED_BUFFER:
		return frameResources.m_resourceViews[handle - 1].m_structuredBufferHandle;
	case gal::DescriptorType::RW_STRUCTURED_BUFFER:
		return frameResources.m_resourceViews[handle - 1].m_rwStructuredBufferHandle;
	default:
		assert(false);
		break;
	}
	return uint32_t();
}

gal::Image *rg::Registry::getImage(ResourceHandle handle) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];
	return frameResources.m_resources[(size_t)handle - 1].m_image;
}

gal::Image *rg::Registry::getImage(ResourceViewHandle handle) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];
	return frameResources.m_resources[(size_t)m_graph->m_viewDescriptions[(size_t)handle - 1].m_resourceHandle - 1].m_image;
}

gal::ImageView *rg::Registry::getImageView(ResourceViewHandle handle) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];
	return frameResources.m_resourceViews[(size_t)handle - 1].m_imageView;
}

gal::Buffer *rg::Registry::getBuffer(ResourceHandle handle) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];
	return frameResources.m_resources[(size_t)handle - 1].m_buffer;
}

gal::Buffer *rg::Registry::getBuffer(ResourceViewHandle handle) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];
	return frameResources.m_resources[(size_t)m_graph->m_viewDescriptions[(size_t)handle - 1].m_resourceHandle - 1].m_buffer;
}

void rg::Registry::map(ResourceViewHandle handle, void **data) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];

	const auto &viewDesc = m_graph->m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph->m_resourceDescriptions[resIdx];
	assert(resDesc.m_hostVisible && !resDesc.m_external);
	frameResources.m_resources[resIdx].m_buffer->map(data);
	*data = ((uint8_t *)*data) + viewDesc.m_offset;
}

void rg::Registry::unmap(ResourceViewHandle handle) const noexcept
{
	const auto &frameResources = m_graph->m_frameResources[m_graph->m_frame % RenderGraph::k_numFrames];

	const auto &viewDesc = m_graph->m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph->m_resourceDescriptions[resIdx];
	assert(resDesc.m_hostVisible && !resDesc.m_external);
	frameResources.m_resources[resIdx].m_buffer->unmap();
}