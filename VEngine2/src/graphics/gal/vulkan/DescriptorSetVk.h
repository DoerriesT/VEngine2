#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace gal
{
	class DescriptorSetLayoutVk : public DescriptorSetLayout
	{
	public:
		explicit DescriptorSetLayoutVk(VkDevice device, uint32_t bindingCount, const VkDescriptorSetLayoutBinding *bindings, const VkDescriptorBindingFlags *bindingFlags);
		DescriptorSetLayoutVk(DescriptorSetLayoutVk &) = delete;
		DescriptorSetLayoutVk(DescriptorSetLayoutVk &&) = delete;
		DescriptorSetLayoutVk &operator= (const DescriptorSetLayoutVk &) = delete;
		DescriptorSetLayoutVk &operator= (const DescriptorSetLayoutVk &&) = delete;
		~DescriptorSetLayoutVk();
		void *getNativeHandle() const override;
		const uint32_t *getTypeCounts() const;
	private:
		VkDevice m_device;
		VkDescriptorSetLayout m_descriptorSetLayout;
		uint32_t m_typeCounts[(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1)];
	};

	class DescriptorSetVk : public DescriptorSet
	{
	public:
		explicit DescriptorSetVk(VkDevice device, VkDescriptorSet descriptorSet, uint32_t dynamicBufferCount);
		void *getNativeHandle() const override;
		void update(uint32_t count, const DescriptorSetUpdate *updates) override;
		uint32_t getDynamicBufferCount() const;

	private:
		VkDevice m_device;
		VkDescriptorSet m_descriptorSet;
		uint32_t m_dynamicBufferCount;
	};

	class DescriptorSetPoolVk : public DescriptorSetPool
	{
	public:
		explicit DescriptorSetPoolVk(VkDevice device, uint32_t maxSets, const DescriptorSetLayoutVk *layout);
		DescriptorSetPoolVk(DescriptorSetPoolVk &) = delete;
		DescriptorSetPoolVk(DescriptorSetPoolVk &&) = delete;
		DescriptorSetPoolVk &operator= (const DescriptorSetPoolVk &) = delete;
		DescriptorSetPoolVk &operator= (const DescriptorSetPoolVk &&) = delete;
		~DescriptorSetPoolVk();
		void *getNativeHandle() const override;
		void allocateDescriptorSets(uint32_t count, DescriptorSet **sets)  override;
		void reset()  override;
	private:
		VkDevice m_device;
		VkDescriptorPool m_descriptorPool;
		const DescriptorSetLayoutVk *m_layout;
		uint32_t m_poolSize;
		uint32_t m_currentOffset;
		char *m_descriptorSetMemory;
	};
}