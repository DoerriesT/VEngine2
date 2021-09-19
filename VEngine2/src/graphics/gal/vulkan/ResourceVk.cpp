#include "ResourceVk.h"
#include "MemoryAllocatorVk.h"
#include "GraphicsDeviceVk.h"
#include "UtilityVk.h"
#include "utility/Utility.h"

gal::SamplerVk::SamplerVk(VkDevice device, const SamplerCreateInfo &createInfo)
	: m_device(device),
	m_sampler(VK_NULL_HANDLE)
{
	VkSamplerCreateInfo createInfoVk{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	createInfoVk.magFilter = UtilityVk::translate(createInfo.m_magFilter);
	createInfoVk.minFilter = UtilityVk::translate(createInfo.m_minFilter);
	createInfoVk.mipmapMode = UtilityVk::translate(createInfo.m_mipmapMode);
	createInfoVk.addressModeU = UtilityVk::translate(createInfo.m_addressModeU);
	createInfoVk.addressModeV = UtilityVk::translate(createInfo.m_addressModeV);
	createInfoVk.addressModeW = UtilityVk::translate(createInfo.m_addressModeW);
	createInfoVk.mipLodBias = createInfo.m_mipLodBias;
	createInfoVk.anisotropyEnable = createInfo.m_anisotropyEnable;
	createInfoVk.maxAnisotropy = createInfo.m_maxAnisotropy;
	createInfoVk.compareEnable = createInfo.m_compareEnable;
	createInfoVk.compareOp = UtilityVk::translate(createInfo.m_compareOp);
	createInfoVk.minLod = createInfo.m_minLod;
	createInfoVk.maxLod = createInfo.m_maxLod;
	createInfoVk.borderColor = UtilityVk::translate(createInfo.m_borderColor);
	createInfoVk.unnormalizedCoordinates = createInfo.m_unnormalizedCoordinates;

	UtilityVk::checkResult(vkCreateSampler(m_device, &createInfoVk, nullptr, &m_sampler), "Failed to create Sampler!");
}

gal::SamplerVk::~SamplerVk()
{
	vkDestroySampler(m_device, m_sampler, nullptr);
}

void *gal::SamplerVk::getNativeHandle() const
{
	return m_sampler;
}

gal::ImageVk::ImageVk(VkImage image, void *allocHandle, const ImageCreateInfo &createInfo)
	:m_image(image),
	m_allocHandle(allocHandle),
	m_description(createInfo)
{
}

void *gal::ImageVk::getNativeHandle() const
{
	return m_image;
}

const gal::ImageCreateInfo &gal::ImageVk::getDescription() const
{
	return m_description;
}

void *gal::ImageVk::getAllocationHandle()
{
	return m_allocHandle;
}

gal::BufferVk::BufferVk(VkBuffer buffer, void *allocHandle, const BufferCreateInfo &createInfo, MemoryAllocatorVk *allocator, GraphicsDeviceVk *device)
	:m_buffer(buffer),
	m_allocHandle(allocHandle),
	m_description(createInfo),
	m_allocator(allocator),
	m_device(device)
{
}

void *gal::BufferVk::getNativeHandle() const
{
	return m_buffer;
}

const gal::BufferCreateInfo &gal::BufferVk::getDescription() const
{
	return m_description;
}

void gal::BufferVk::map(void **data)
{
	m_allocator->mapMemory(static_cast<AllocationHandleVk>(m_allocHandle), data);
}

void gal::BufferVk::unmap()
{
	m_allocator->unmapMemory(static_cast<AllocationHandleVk>(m_allocHandle));
}

void gal::BufferVk::invalidate(uint32_t count, const MemoryRange *ranges)
{
	const auto allocInfo = m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle));
	const uint64_t nonCoherentAtomSize = m_device->getDeviceProperties().limits.nonCoherentAtomSize;

	constexpr uint32_t batchSize = 16;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t countVk = eastl::min(batchSize, count - i * batchSize);
		VkMappedMemoryRange rangesVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, allocInfo.m_memory, allocInfo.m_offset + range.m_offset, range.m_size };
			rangesVk[j].offset = util::alignDown(rangesVk[j].offset, nonCoherentAtomSize);
			rangesVk[j].size = util::alignUp(rangesVk[j].size, nonCoherentAtomSize);
		}
		UtilityVk::checkResult(vkInvalidateMappedMemoryRanges(m_device->getDevice(), countVk, rangesVk), "Failed to invalidate mapped memory ranges!");
	}
}

void gal::BufferVk::flush(uint32_t count, const MemoryRange *ranges)
{
	const auto allocInfo = m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle));
	const uint64_t nonCoherentAtomSize = m_device->getDeviceProperties().limits.nonCoherentAtomSize;

	constexpr uint32_t batchSize = 16;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t countVk = eastl::min(batchSize, count - i * batchSize);
		VkMappedMemoryRange rangesVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, allocInfo.m_memory, allocInfo.m_offset + range.m_offset, range.m_size };
			rangesVk[j].offset = util::alignDown(rangesVk[j].offset, nonCoherentAtomSize);
			rangesVk[j].size = util::alignUp(rangesVk[j].size, nonCoherentAtomSize);
		}
		UtilityVk::checkResult(vkFlushMappedMemoryRanges(m_device->getDevice(), countVk, rangesVk), "Failed to invalidate mapped memory ranges!");
	}
}

void *gal::BufferVk::getAllocationHandle()
{
	return m_allocHandle;
}

VkDeviceMemory gal::BufferVk::getMemory() const
{
	return m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle)).m_memory;
}

VkDeviceSize gal::BufferVk::getOffset() const
{
	return m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle)).m_offset;
}

gal::ImageViewVk::ImageViewVk(VkDevice device, const ImageViewCreateInfo &createInfo)
	:m_device(device),
	m_imageView(VK_NULL_HANDLE),
	m_description(createInfo)
{
	const auto *imageVk = dynamic_cast<const ImageVk *>(createInfo.m_image);
	assert(imageVk);

	VkImageViewCreateInfo createInfoVk{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfoVk.image = (VkImage)imageVk->getNativeHandle();
	createInfoVk.viewType = UtilityVk::translate(createInfo.m_viewType);
	createInfoVk.format = UtilityVk::translate(createInfo.m_format == Format::UNDEFINED ? imageVk->getDescription().m_format : createInfo.m_format);
	createInfoVk.components.r = UtilityVk::translate(createInfo.m_components.m_r);
	createInfoVk.components.g = UtilityVk::translate(createInfo.m_components.m_g);
	createInfoVk.components.b = UtilityVk::translate(createInfo.m_components.m_b);
	createInfoVk.components.a = UtilityVk::translate(createInfo.m_components.m_a);
	createInfoVk.subresourceRange =
	{
		UtilityVk::getImageAspectMask(UtilityVk::translate(imageVk->getDescription().m_format)),
		createInfo.m_baseMipLevel,
		createInfo.m_levelCount,
		createInfo.m_baseArrayLayer,
		createInfo.m_layerCount
	};

	UtilityVk::checkResult(vkCreateImageView(m_device, &createInfoVk, nullptr, &m_imageView), "Failed to create Image View!");
}

gal::ImageViewVk::~ImageViewVk()
{
	vkDestroyImageView(m_device, m_imageView, nullptr);
}

void *gal::ImageViewVk::getNativeHandle() const
{
	return m_imageView;
}

const gal::Image *gal::ImageViewVk::getImage() const
{
	return m_description.m_image;
}

const gal::ImageViewCreateInfo &gal::ImageViewVk::getDescription() const
{
	return m_description;
}

gal::BufferViewVk::BufferViewVk(VkDevice device, const BufferViewCreateInfo &createInfo)
	:m_device(device),
	m_bufferView(VK_NULL_HANDLE),
	m_description(createInfo)
{
	const auto *bufferVk = dynamic_cast<const BufferVk *>(createInfo.m_buffer);
	assert(bufferVk);

	VkBufferViewCreateInfo createInfoVk{ VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
	createInfoVk.buffer = (VkBuffer)bufferVk->getNativeHandle();
	createInfoVk.format = UtilityVk::translate(createInfo.m_format);
	createInfoVk.offset = createInfo.m_offset;
	createInfoVk.range = createInfo.m_range;

	UtilityVk::checkResult(vkCreateBufferView(m_device, &createInfoVk, nullptr, &m_bufferView), "Failed to create Buffer View!");
}

gal::BufferViewVk::~BufferViewVk()
{
	vkDestroyBufferView(m_device, m_bufferView, nullptr);
}

void *gal::BufferViewVk::getNativeHandle() const
{
	return m_bufferView;
}

const gal::Buffer *gal::BufferViewVk::getBuffer() const
{
	return m_description.m_buffer;
}

const gal::BufferViewCreateInfo &gal::BufferViewVk::getDescription() const
{
	return m_description;
}
