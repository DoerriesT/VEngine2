#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "utility/ObjectPool.h"
#include "utility/TLSFAllocator.h"
#include <d3d12.h>
#include "QueueDx12.h"
#include "PipelineDx12.h"
#include "CommandListPoolDx12.h"
#include "ResourceDx12.h"
#include "SemaphoreDx12.h"
#include "QueryPoolDx12.h"
#include "DescriptorSetDx12.h"

namespace D3D12MA
{
	class Allocator;
}

namespace gal
{
	class SwapChainDx12;

	class GraphicsDeviceDx12 : public GraphicsDevice
	{
	public:
		explicit GraphicsDeviceDx12(void *windowHandle, bool debugLayer);
		GraphicsDeviceDx12(GraphicsDeviceDx12 &) = delete;
		GraphicsDeviceDx12(GraphicsDeviceDx12 &&) = delete;
		GraphicsDeviceDx12 &operator= (const GraphicsDeviceDx12 &) = delete;
		GraphicsDeviceDx12 &operator= (const GraphicsDeviceDx12 &&) = delete;
		~GraphicsDeviceDx12();
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
		uint64_t getBufferCopyRowPitchAlignment() const override;
		float getMaxSamplerAnisotropy() const override;
		void *getProfilingContext() const override;

	private:
		ID3D12Device *m_device;
		QueueDx12 m_graphicsQueue;
		QueueDx12 m_computeQueue;
		QueueDx12 m_transferQueue;
		void *m_windowHandle;
		D3D12MA::Allocator *m_gpuMemoryAllocator;
		SwapChainDx12 *m_swapChain;
		CommandListRecordContextDx12 m_cmdListRecordContext;
		ID3D12DescriptorHeap *m_cpuDescriptorHeap;
		ID3D12DescriptorHeap *m_cpuSamplerDescriptorHeap;
		ID3D12DescriptorHeap *m_cpuRTVDescriptorHeap;
		ID3D12DescriptorHeap *m_cpuDSVDescriptorHeap;
		TLSFAllocator m_gpuDescriptorAllocator;
		TLSFAllocator m_gpuSamplerDescriptorAllocator;
		TLSFAllocator m_cpuDescriptorAllocator;
		TLSFAllocator m_cpuSamplerDescriptorAllocator;
		TLSFAllocator m_cpuRTVDescriptorAllocator;
		TLSFAllocator m_cpuDSVDescriptorAllocator;
		UINT m_descriptorIncrementSizes[4];
		DynamicObjectMemoryPool<GraphicsPipelineDx12> m_graphicsPipelineMemoryPool;
		DynamicObjectMemoryPool<ComputePipelineDx12> m_computePipelineMemoryPool;
		DynamicObjectMemoryPool<CommandListPoolDx12> m_commandListPoolMemoryPool;
		DynamicObjectMemoryPool<ImageDx12> m_imageMemoryPool;
		DynamicObjectMemoryPool<BufferDx12> m_bufferMemoryPool;
		DynamicObjectMemoryPool<ImageViewDx12> m_imageViewMemoryPool;
		DynamicObjectMemoryPool<BufferViewDx12> m_bufferViewMemoryPool;
		DynamicObjectMemoryPool<SamplerDx12> m_samplerMemoryPool;
		DynamicObjectMemoryPool<SemaphoreDx12> m_semaphoreMemoryPool;
		DynamicObjectMemoryPool<QueryPoolDx12> m_queryPoolMemoryPool;
		DynamicObjectMemoryPool<DescriptorSetPoolDx12> m_descriptorSetPoolMemoryPool;
		DynamicObjectMemoryPool<DescriptorSetLayoutDx12> m_descriptorSetLayoutMemoryPool;
		bool m_debugLayers;
		void *m_profilingContext = nullptr;
	};
}