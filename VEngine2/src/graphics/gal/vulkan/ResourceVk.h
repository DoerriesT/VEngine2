#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace gal
{
	class GraphicsDeviceVk;
	class MemoryAllocatorVk;

	class SamplerVk : public Sampler
	{
	public:
		explicit SamplerVk(VkDevice device, const SamplerCreateInfo &createInfo);
		SamplerVk(SamplerVk &) = delete;
		SamplerVk(SamplerVk &&) = delete;
		SamplerVk &operator= (const SamplerVk &) = delete;
		SamplerVk &operator= (const SamplerVk &&) = delete;
		~SamplerVk();
		void *getNativeHandle() const override;

	private:
		VkDevice m_device;
		VkSampler m_sampler;
	};

	class ImageVk : public Image
	{
	public:
		explicit ImageVk(VkImage image, void *allocHandle, const ImageCreateInfo &createInfo);
		void *getNativeHandle() const override;
		const ImageCreateInfo &getDescription() const override;
		void *getAllocationHandle();

	private:
		VkImage m_image;
		void *m_allocHandle;
		ImageCreateInfo m_description;
	};

	class BufferVk : public Buffer
	{
	public:
		explicit BufferVk(VkBuffer buffer, void *allocHandle, const BufferCreateInfo &createInfo, MemoryAllocatorVk *allocator, GraphicsDeviceVk *device);
		void *getNativeHandle() const override;
		const BufferCreateInfo &getDescription() const override;
		void map(void **data) override;
		void unmap() override;
		void invalidate(uint32_t count, const MemoryRange *ranges) override;
		void flush(uint32_t count, const MemoryRange *ranges) override;
		void *getAllocationHandle();
		VkDeviceMemory getMemory() const;
		VkDeviceSize getOffset() const;

	private:
		VkBuffer m_buffer;
		void *m_allocHandle;
		BufferCreateInfo m_description;
		MemoryAllocatorVk *m_allocator;
		GraphicsDeviceVk *m_device;
	};

	class ImageViewVk : public ImageView
	{
	public:
		explicit ImageViewVk(VkDevice device, const ImageViewCreateInfo &createInfo);
		ImageViewVk(ImageViewVk &) = delete;
		ImageViewVk(ImageViewVk &&) = delete;
		ImageViewVk &operator= (const ImageViewVk &) = delete;
		ImageViewVk &operator= (const ImageViewVk &&) = delete;
		~ImageViewVk();
		void *getNativeHandle() const override;
		const Image *getImage() const override;
		const ImageViewCreateInfo &getDescription() const override;

	private:
		VkDevice m_device;
		VkImageView m_imageView;
		ImageViewCreateInfo m_description;
	};

	class BufferViewVk : public BufferView
	{
	public:
		explicit BufferViewVk(VkDevice device, const BufferViewCreateInfo &createInfo);
		BufferViewVk(BufferViewVk &) = delete;
		BufferViewVk(BufferViewVk &&) = delete;
		BufferViewVk &operator= (const BufferViewVk &) = delete;
		BufferViewVk &operator= (const BufferViewVk &&) = delete;
		~BufferViewVk();
		void *getNativeHandle() const override;
		const Buffer *getBuffer() const override;
		const BufferViewCreateInfo &getDescription() const override;

	private:
		VkDevice m_device;
		VkBufferView m_bufferView;
		BufferViewCreateInfo m_description;
	};
}