#include "DescriptorSetVk.h"
#include "UtilityVk.h"
#include "ResourceVk.h"
#include <assert.h>
#include <EASTL/vector.h>
#include "utility/Utility.h"
#include "utility/allocator/DefaultAllocator.h"
#include "../Initializers.h"

gal::DescriptorSetLayoutVk::DescriptorSetLayoutVk(VkDevice device, uint32_t bindingCount, const VkDescriptorSetLayoutBinding *bindings, const VkDescriptorBindingFlags *bindingFlags)
	:m_device(device),
	m_descriptorSetLayout(VK_NULL_HANDLE),
	m_typeCounts()
{
	for (uint32_t i = 0; i < bindingCount; ++i)
	{
		assert(static_cast<size_t>(bindings[i].descriptorType) < eastl::size(m_typeCounts));
		m_typeCounts[static_cast<size_t>(bindings[i].descriptorType)] += bindings[i].descriptorCount;
	}

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
	bindingFlagsCreateInfo.bindingCount = bindingCount;
	bindingFlagsCreateInfo.pBindingFlags = bindingFlags;

	VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, bindingFlags ? &bindingFlagsCreateInfo : nullptr };
	createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	createInfo.bindingCount = bindingCount;
	createInfo.pBindings = bindings;

	UtilityVk::checkResult(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &m_descriptorSetLayout), "Failed to create DescriptorSetLayout!");
}

gal::DescriptorSetLayoutVk::~DescriptorSetLayoutVk()
{
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
}

void *gal::DescriptorSetLayoutVk::getNativeHandle() const
{
	return m_descriptorSetLayout;
}

const uint32_t *gal::DescriptorSetLayoutVk::getTypeCounts() const
{
	return m_typeCounts;
}

gal::DescriptorSetVk::DescriptorSetVk(VkDevice device, VkDescriptorSet descriptorSet, uint32_t dynamicBufferCount)
	:m_device(device),
	m_descriptorSet(descriptorSet),
	m_dynamicBufferCount(dynamicBufferCount)
{
}

void *gal::DescriptorSetVk::getNativeHandle() const
{
	return m_descriptorSet;
}

void gal::DescriptorSetVk::update(uint32_t count, const DescriptorSetUpdate *updates)
{
	constexpr uint32_t batchSize = 16;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = eastl::min(batchSize, count - i * batchSize);

		eastl::vector<VkDescriptorImageInfo> imageInfos;
		eastl::vector<VkDescriptorBufferInfo> bufferInfos;
		eastl::vector<VkBufferView> texelBufferViews;

		size_t imageInfoReserveCount = 0;
		size_t bufferInfoReserveCount = 0;
		size_t texelBufferViewsReserveCount = 0;
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &update = updates[i * batchSize + j];
			switch (update.m_descriptorType)
			{
			case DescriptorType::SAMPLER:
			case DescriptorType::TEXTURE:
			case DescriptorType::RW_TEXTURE:
				imageInfoReserveCount += update.m_descriptorCount;
				break;
			case DescriptorType::TYPED_BUFFER:
			case DescriptorType::RW_TYPED_BUFFER:
				texelBufferViewsReserveCount += update.m_descriptorCount;
				break;
			case DescriptorType::CONSTANT_BUFFER:
			case DescriptorType::BYTE_BUFFER:
			case DescriptorType::RW_BYTE_BUFFER:
			case DescriptorType::STRUCTURED_BUFFER:
			case DescriptorType::RW_STRUCTURED_BUFFER:
				bufferInfoReserveCount += update.m_descriptorCount;
				break;
			case DescriptorType::OFFSET_CONSTANT_BUFFER:
				bufferInfoReserveCount += 1;
				break;
			default:
				assert(false);
				break;
			}
		}

		imageInfos.reserve(imageInfoReserveCount);
		bufferInfos.reserve(bufferInfoReserveCount);
		texelBufferViews.reserve(texelBufferViewsReserveCount);

		//union InfoData
		//{
		//	VkDescriptorImageInfo m_imageInfo;
		//	VkDescriptorBufferInfo m_bufferInfo;
		//	VkBufferView m_texelBufferView;
		//};
		//
		//InfoData infoDataVk[batchSize];
		VkWriteDescriptorSet writesVk[batchSize];

		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &update = updates[i * batchSize + j];

			auto &write = writesVk[j];
			write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstSet = m_descriptorSet;
			write.dstBinding = update.m_dstBinding;
			write.dstArrayElement = update.m_dstArrayElement;
			write.descriptorCount = update.m_descriptorCount;

			//auto &info = infoDataVk[j];

			switch (update.m_descriptorType)
			{
			case DescriptorType::SAMPLER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const Sampler *sampler = update.m_samplers ? update.m_samplers[k] : update.m_sampler;
					imageInfos.push_back({ (VkSampler)sampler->getNativeHandle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				write.pImageInfo = imageInfos.data() + imageInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::TEXTURE:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const ImageView *view = update.m_imageViews ? update.m_imageViews[k] : update.m_imageView;
					const auto format = view->getDescription().m_format;
					const auto layout = Initializers::isDepthFormat(format) || Initializers::isStencilFormat(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageInfos.push_back({ VK_NULL_HANDLE, (VkImageView)view->getNativeHandle(), layout });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				write.pImageInfo = imageInfos.data() + imageInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::RW_TEXTURE:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const ImageView *view = update.m_imageViews ? update.m_imageViews[k] : update.m_imageView;
					imageInfos.push_back({ VK_NULL_HANDLE, (VkImageView)view->getNativeHandle(), VK_IMAGE_LAYOUT_GENERAL });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				write.pImageInfo = imageInfos.data() + imageInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::TYPED_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const BufferView *view = update.m_bufferViews ? update.m_bufferViews[k] : update.m_bufferView;
					texelBufferViews.push_back((VkBufferView)view->getNativeHandle());
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				write.pTexelBufferView = texelBufferViews.data() + texelBufferViews.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::RW_TYPED_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const BufferView *view = update.m_bufferViews ? update.m_bufferViews[k] : update.m_bufferView;
					texelBufferViews.push_back((VkBufferView)view->getNativeHandle());
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				write.pTexelBufferView = texelBufferViews.data() + texelBufferViews.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::CONSTANT_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const auto &info = update.m_bufferInfo ? update.m_bufferInfo[k] : update.m_bufferInfo1;
					const auto *bufferVk = dynamic_cast<const BufferVk *>(info.m_buffer);
					assert(bufferVk);
					bufferInfos.push_back({ (VkBuffer)bufferVk->getNativeHandle(), info.m_offset, info.m_range });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.pBufferInfo = bufferInfos.data() + bufferInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::BYTE_BUFFER:
			case DescriptorType::RW_BYTE_BUFFER:
			case DescriptorType::STRUCTURED_BUFFER:
			case DescriptorType::RW_STRUCTURED_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const auto &info = update.m_bufferInfo ? update.m_bufferInfo[k] : update.m_bufferInfo1;
					const auto *bufferVk = dynamic_cast<const BufferVk *>(info.m_buffer);
					assert(bufferVk);
					bufferInfos.push_back({ (VkBuffer)bufferVk->getNativeHandle(), info.m_offset, info.m_range });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				write.pBufferInfo = bufferInfos.data() + bufferInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::OFFSET_CONSTANT_BUFFER:
			{
				const auto &info = update.m_bufferInfo ? update.m_bufferInfo[0] : update.m_bufferInfo1;
				const auto *bufferVk = dynamic_cast<const BufferVk *>(info.m_buffer);
				assert(bufferVk);
				bufferInfos.push_back({ (VkBuffer)bufferVk->getNativeHandle(), info.m_offset, info.m_range });
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				write.pBufferInfo = bufferInfos.data() + bufferInfos.size() - 1;
				break;
			}
			default:
				assert(false);
				break;
			}
		}

		vkUpdateDescriptorSets(m_device, countVk, writesVk, 0, nullptr);
	}
}

uint32_t gal::DescriptorSetVk::getDynamicBufferCount() const
{
	return m_dynamicBufferCount;
}

gal::DescriptorSetPoolVk::DescriptorSetPoolVk(VkDevice device, uint32_t maxSets, const DescriptorSetLayoutVk *layout)
	:m_device(device),
	m_descriptorPool(VK_NULL_HANDLE),
	m_layout(layout),
	m_poolSize(maxSets),
	m_currentOffset(),
	m_descriptorSetMemory()
{
	VkDescriptorPoolSize poolSizes[(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1)];

	uint32_t poolSizeCount = 0;

	const uint32_t *typeCounts = layout->getTypeCounts();

	for (size_t i = 0; i < (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1); ++i)
	{
		if (typeCounts[i])
		{
			poolSizes[poolSizeCount].type = static_cast<VkDescriptorType>(i);
			poolSizes[poolSizeCount].descriptorCount = typeCounts[i] * maxSets;
			++poolSizeCount;
		}
	}

	assert(poolSizeCount);

	VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCreateInfo.maxSets = maxSets;
	poolCreateInfo.poolSizeCount = poolSizeCount;
	poolCreateInfo.pPoolSizes = poolSizes;

	UtilityVk::checkResult(vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_descriptorPool), "Failed to create DescriptorPool!");

	// allocate memory to placement new DescriptorSetVk
	m_descriptorSetMemory = (char *)DefaultAllocator::get()->allocate(sizeof(DescriptorSetVk) * m_poolSize, alignof(DescriptorSetVk), 0);
}

gal::DescriptorSetPoolVk::~DescriptorSetPoolVk()
{
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	// normally we would have to manually call the destructor on all currently allocated sets,
	// which would require keeping a list of all sets. since DescriptorSetVk is a POD, we can
	// simply scrap the backing memory without first doing this.

	DefaultAllocator::get()->deallocate(m_descriptorSetMemory, sizeof(DescriptorSetVk) * m_poolSize);
	m_descriptorSetMemory = nullptr;
}

void *gal::DescriptorSetPoolVk::getNativeHandle() const
{
	return m_descriptorPool;
}

void gal::DescriptorSetPoolVk::allocateDescriptorSets(uint32_t count, DescriptorSet **sets)
{
	if (m_currentOffset + count > m_poolSize)
	{
		util::fatalExit("Tried to allocate more descriptor sets from descriptor set pool than available!", EXIT_FAILURE);
	}

	VkDescriptorSetLayout layoutVk = (VkDescriptorSetLayout)m_layout->getNativeHandle();
	const uint32_t dynamicBufferCount = m_layout->getTypeCounts()[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC];

	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = eastl::min(batchSize, count - i * batchSize);
		VkDescriptorSetLayout layoutsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			layoutsVk[j] = layoutVk;
		}

		VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = countVk;
		allocInfo.pSetLayouts = layoutsVk;

		VkDescriptorSet setsVk[batchSize];

		UtilityVk::checkResult(vkAllocateDescriptorSets(m_device, &allocInfo, setsVk), "Failed to allocate Descriptor Sets from Descriptor Pool!");

		for (uint32_t j = 0; j < countVk; ++j)
		{
			sets[i * batchSize + j] = new(m_descriptorSetMemory + sizeof(DescriptorSetVk) * m_currentOffset) DescriptorSetVk(m_device, setsVk[j], dynamicBufferCount);
			++m_currentOffset;
		}
	}
}

void gal::DescriptorSetPoolVk::reset()
{
	UtilityVk::checkResult(vkResetDescriptorPool(m_device, m_descriptorPool, 0), "Failed to reset descriptor pool!");

	// DescriptorSetVk is a POD class, so we can simply reset the allocator offset of the backing memory
	// and dont need to call the destructors
	m_currentOffset = 0;
}