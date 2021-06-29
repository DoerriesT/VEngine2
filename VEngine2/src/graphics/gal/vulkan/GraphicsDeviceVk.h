#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "QueueVk.h"
#include "Utility/ObjectPool.h"
#include "PipelineVk.h"
#include "CommandListPoolVk.h"
#include "ResourceVk.h"
#include "SemaphoreVk.h"
#include "QueryPoolVk.h"
#include "DescriptorSetVk.h"

namespace gal
{
	class SwapChainVk;
	class RenderPassCacheVk;
	class FramebufferCacheVk;
	class MemoryAllocatorVk;
	struct RenderPassDescriptionVk;
	struct FramebufferDescriptionVk;

	class GraphicsDeviceVk : public GraphicsDevice
	{
	public:
		explicit GraphicsDeviceVk(void *windowHandle, bool debugLayer);
		GraphicsDeviceVk(GraphicsDeviceVk &) = delete;
		GraphicsDeviceVk(GraphicsDeviceVk &&) = delete;
		GraphicsDeviceVk &operator= (const GraphicsDeviceVk &) = delete;
		GraphicsDeviceVk &operator= (const GraphicsDeviceVk &&) = delete;
		~GraphicsDeviceVk();
		void createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines) override;
		void createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines) override;
		void destroyGraphicsPipeline(GraphicsPipeline *pipeline) override;
		void destroyComputePipeline(ComputePipeline *pipeline) override;
		void createCommandListPool(const Queue *queue, CommandListPool **commandListPool) override;
		void destroyCommandListPool(CommandListPool *commandListPool) override;
		void createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool) override;
		void destroyQueryPool(QueryPool *queryPool) override;
		void createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image) override;
		void createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer) override;
		void destroyImage(Image *image) override;
		void destroyBuffer(Buffer *buffer) override;
		void createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView) override;
		void createImageView(Image *image, ImageView **imageView) override;
		void createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView) override;
		void destroyImageView(ImageView *imageView) override;
		void destroyBufferView(BufferView *bufferView) override;
		void createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler) override;
		void destroySampler(Sampler *sampler) override;
		void createSemaphore(uint64_t initialValue, Semaphore **semaphore) override;
		void destroySemaphore(Semaphore *semaphore) override;
		void createDescriptorSetPool(uint32_t maxSets, const DescriptorSetLayout *descriptorSetLayout, DescriptorSetPool **descriptorSetPool)  override;
		void destroyDescriptorSetPool(DescriptorSetPool *descriptorSetPool)  override;
		void createDescriptorSetLayout(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings, DescriptorSetLayout **descriptorSetLayout) override;
		void destroyDescriptorSetLayout(DescriptorSetLayout *descriptorSetLayout) override;
		void createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode, SwapChain **swapChain) override;
		void destroySwapChain() override;
		void waitIdle() override;
		void setDebugObjectName(ObjectType objectType, void *object, const char *name) override;
		Queue *getGraphicsQueue() override;
		Queue *getComputeQueue() override;
		Queue *getTransferQueue() override;
		uint64_t getBufferAlignment(DescriptorType bufferType, uint64_t elementSize) const override;
		uint64_t getMinUniformBufferOffsetAlignment() const override;
		uint64_t getMinStorageBufferOffsetAlignment() const override;
		uint64_t getBufferCopyOffsetAlignment() const  override;
		uint64_t getBufferCopyRowPitchAlignment() const  override;
		float getMaxSamplerAnisotropy() const;
		VkDevice getDevice() const;
		VkRenderPass getRenderPass(const RenderPassDescriptionVk &renderPassDescription);
		VkFramebuffer getFramebuffer(const FramebufferDescriptionVk &framebufferDescription);
		const VkPhysicalDeviceProperties &getDeviceProperties() const;

	private:
		VkInstance m_instance;
		VkDevice m_device;
		VkPhysicalDevice m_physicalDevice;
		VkPhysicalDeviceFeatures m_features;
		VkPhysicalDeviceFeatures m_enabledFeatures;
		VkPhysicalDeviceProperties m_properties;
		VkPhysicalDeviceSubgroupProperties m_subgroupProperties;
		QueueVk m_graphicsQueue;
		QueueVk m_computeQueue;
		QueueVk m_transferQueue;
		VkSurfaceKHR m_surface;
		VkDebugUtilsMessengerEXT m_debugUtilsMessenger;
		RenderPassCacheVk *m_renderPassCache;
		FramebufferCacheVk *m_framebufferCache;
		MemoryAllocatorVk *m_gpuMemoryAllocator;
		SwapChainVk *m_swapChain;
		DynamicObjectMemoryPool<GraphicsPipelineVk> m_graphicsPipelineMemoryPool;
		DynamicObjectMemoryPool<ComputePipelineVk> m_computePipelineMemoryPool;
		DynamicObjectMemoryPool<CommandListPoolVk> m_commandListPoolMemoryPool;
		DynamicObjectMemoryPool<ImageVk> m_imageMemoryPool;
		DynamicObjectMemoryPool<BufferVk> m_bufferMemoryPool;
		DynamicObjectMemoryPool<ImageViewVk> m_imageViewMemoryPool;
		DynamicObjectMemoryPool<BufferViewVk> m_bufferViewMemoryPool;
		DynamicObjectMemoryPool<SamplerVk> m_samplerMemoryPool;
		DynamicObjectMemoryPool<SemaphoreVk> m_semaphoreMemoryPool;
		DynamicObjectMemoryPool<QueryPoolVk> m_queryPoolMemoryPool;
		DynamicObjectMemoryPool<DescriptorSetPoolVk> m_descriptorSetPoolMemoryPool;
		DynamicObjectMemoryPool<DescriptorSetLayoutVk> m_descriptorSetLayoutMemoryPool;
		bool m_debugLayers;
	};
}