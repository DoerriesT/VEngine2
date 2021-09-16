#define NOMINMAX
#include "GraphicsDeviceDx12.h"
#include "assert.h"
#include "SwapChainDx12.h"
#include "UtilityDx12.h"
#include "utility/Utility.h"
#include "D3D12MemAlloc.h"
#include "./../Initializers.h"
#include <tracy/TracyD3D12.hpp>

#define GPU_DESCRIPTOR_HEAP_SIZE (1000000)
#define GPU_SAMPLER_DESCRIPTOR_HEAP_SIZE (1024)
#define CPU_DESCRIPTOR_HEAP_SIZE (1000000)
#define CPU_SAMPLER_DESCRIPTOR_HEAP_SIZE (1024)
#define CPU_RTV_DESCRIPTOR_HEAP_SIZE (1024)
#define CPU_DSV_DESCRIPTOR_HEAP_SIZE (1024)

using namespace gal;

gal::GraphicsDeviceDx12::GraphicsDeviceDx12(void *windowHandle, bool debugLayer)
	:m_device(),
	m_graphicsQueue(),
	m_computeQueue(),
	m_transferQueue(),
	m_windowHandle(windowHandle),
	//m_gpuMemoryAllocator(),
	m_swapChain(),
	m_cmdListRecordContext(),
	m_graphicsPipelineMemoryPool(64),
	m_computePipelineMemoryPool(64),
	m_commandListPoolMemoryPool(32),
	m_gpuDescriptorAllocator(GPU_DESCRIPTOR_HEAP_SIZE, 1),
	m_gpuSamplerDescriptorAllocator(GPU_SAMPLER_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuDescriptorAllocator(CPU_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuSamplerDescriptorAllocator(CPU_SAMPLER_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuRTVDescriptorAllocator(CPU_RTV_DESCRIPTOR_HEAP_SIZE, 1),
	m_cpuDSVDescriptorAllocator(CPU_DSV_DESCRIPTOR_HEAP_SIZE, 1),
	m_imageMemoryPool(256),
	m_bufferMemoryPool(256),
	m_imageViewMemoryPool(512),
	m_bufferViewMemoryPool(32),
	m_samplerMemoryPool(16),
	m_semaphoreMemoryPool(16),
	m_queryPoolMemoryPool(16),
	m_descriptorSetPoolMemoryPool(16),
	m_descriptorSetLayoutMemoryPool(8),
	m_debugLayers(debugLayer)
{
	// Enable the D3D12 debug layer.
	if (m_debugLayers)
	{
		ID3D12Debug *debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void **)&debugController)))
		{
			debugController->EnableDebugLayer();
		}
		debugController->Release();
	}

	// get adapter
	IDXGIAdapter4 *dxgiAdapter4 = nullptr;
	{
		IDXGIFactory4 *dxgiFactory;
		UINT createFactoryFlags = 0;

		if (m_debugLayers)
		{
			createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
		}

		UtilityDx12::checkResult(CreateDXGIFactory2(createFactoryFlags, __uuidof(IDXGIFactory4), (void **)&dxgiFactory), "Failed to create DXGIFactory!");

		IDXGIAdapter1 *dxgiAdapter1;

		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// check to see if the adapter can create a D3D12 device without actually
			// creating it. the adapter with the largest dedicated video memory is favored
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				UtilityDx12::checkResult(dxgiAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void **)&dxgiAdapter4), "Failed to create DXGIAdapter4!");
			}
		}
	}

	// create device
	{
		ID3D12Device2 *d3d12Device2;
		UtilityDx12::checkResult(D3D12CreateDevice(dxgiAdapter4, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), (void **)&d3d12Device2), "Failed to create device!");

		if (m_debugLayers)
		{
			ID3D12InfoQueue *pInfoQueue;
			if (SUCCEEDED(d3d12Device2->QueryInterface(__uuidof(ID3D12InfoQueue), (void **)&pInfoQueue)))
			{
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

				// suppress whole categories of messages
				//D3D12_MESSAGE_CATEGORY Categories[] = {};

				// suppress messages based on their severity level
				D3D12_MESSAGE_SEVERITY Severities[] =
				{
					D3D12_MESSAGE_SEVERITY_INFO
				};

				// suppress individual messages by their ID
				D3D12_MESSAGE_ID DenyIds[] =
				{
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
					D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
					D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE
				};

				D3D12_INFO_QUEUE_FILTER NewFilter = {};
				//NewFilter.DenyList.NumCategories = _countof(Categories);
				//NewFilter.DenyList.pCategoryList = Categories;
				NewFilter.DenyList.NumSeverities = _countof(Severities);
				NewFilter.DenyList.pSeverityList = Severities;
				NewFilter.DenyList.NumIDs = _countof(DenyIds);
				NewFilter.DenyList.pIDList = DenyIds;

				//UtilityDx12::checkResult(pInfoQueue->PushStorageFilter(&NewFilter), "Failed to set info queue filter!");
				pInfoQueue->Release();
			}
		}

		m_device = d3d12Device2;
	}

	// create memory allocator
	{
		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED;
		allocatorDesc.pDevice = m_device;
		allocatorDesc.pAdapter = dxgiAdapter4;

		UtilityDx12::checkResult(D3D12MA::CreateAllocator(&allocatorDesc, &m_gpuMemoryAllocator), "Failed to create GPU memory allocator!");
	}

	dxgiAdapter4->Release();

	// create queues
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		// graphics queue
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			ID3D12CommandQueue *queue;
			UtilityDx12::checkResult(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void **)&queue), "Failed to create graphics queue!");
			Semaphore *waitIdleSemaphore;
			createSemaphore(0, &waitIdleSemaphore);
			m_graphicsQueue.init(queue, QueueType::GRAPHICS, 64, true, waitIdleSemaphore);
		}

		// compute queue
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

			ID3D12CommandQueue *queue;
			UtilityDx12::checkResult(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void **)&queue), "Failed to create compute queue!");
			Semaphore *waitIdleSemaphore;
			createSemaphore(0, &waitIdleSemaphore);
			m_computeQueue.init(queue, QueueType::COMPUTE, 64, true, waitIdleSemaphore);
		}

		// transfer queue
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

			ID3D12CommandQueue *queue;
			UtilityDx12::checkResult(m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void **)&queue), "Failed to create transfer queue!");
			Semaphore *waitIdleSemaphore;
			createSemaphore(0, &waitIdleSemaphore);
			m_transferQueue.init(queue, QueueType::TRANSFER, 0, false, waitIdleSemaphore);
		}
	}

	// create descriptor heaps
	{
		// cpu heaps
		{
			// cpu heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuHeapDesc{};
			cpuHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			cpuHeapDesc.NumDescriptors = CPU_DESCRIPTOR_HEAP_SIZE;
			cpuHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuDescriptorHeap), "Failed to create descriptor heap");
			m_cmdListRecordContext.m_cpuDescriptorHeap = m_cpuDescriptorHeap;

			// cpu sampler heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuSamplerHeapDesc{};
			cpuSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			cpuSamplerHeapDesc.NumDescriptors = CPU_SAMPLER_DESCRIPTOR_HEAP_SIZE;
			cpuSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuSamplerHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuSamplerHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuSamplerDescriptorHeap), "Failed to create descriptor heap");

			// cpu rtv heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuRtvHeapDesc{};
			cpuRtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			cpuRtvHeapDesc.NumDescriptors = CPU_RTV_DESCRIPTOR_HEAP_SIZE;
			cpuRtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuRtvHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuRtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuRTVDescriptorHeap), "Failed to create descriptor heap");

			// cpu dsv heap
			D3D12_DESCRIPTOR_HEAP_DESC cpuDsvHeapDesc{};
			cpuDsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			cpuDsvHeapDesc.NumDescriptors = CPU_DSV_DESCRIPTOR_HEAP_SIZE;
			cpuDsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			cpuDsvHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&cpuDsvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cpuDSVDescriptorHeap), "Failed to create descriptor heap");
			m_cmdListRecordContext.m_dsvDescriptorHeap = m_cpuDSVDescriptorHeap;
		}

		// gpu heaps
		{
			// gpu heap
			D3D12_DESCRIPTOR_HEAP_DESC gpuHeapDesc{};
			gpuHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			gpuHeapDesc.NumDescriptors = GPU_DESCRIPTOR_HEAP_SIZE;
			gpuHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			gpuHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&gpuHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cmdListRecordContext.m_gpuDescriptorHeap), "Failed to create descriptor heap");

			// gpu sampler heap
			D3D12_DESCRIPTOR_HEAP_DESC gpuSamplerHeapDesc{};
			gpuSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			gpuSamplerHeapDesc.NumDescriptors = GPU_SAMPLER_DESCRIPTOR_HEAP_SIZE;
			gpuSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			gpuSamplerHeapDesc.NodeMask = 0;

			UtilityDx12::checkResult(m_device->CreateDescriptorHeap(&gpuSamplerHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&m_cmdListRecordContext.m_gpuSamplerDescriptorHeap), "Failed to create descriptor heap");
		}

		m_cmdListRecordContext.m_descriptorIncrementSizes[0] = m_descriptorIncrementSizes[0] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_cmdListRecordContext.m_descriptorIncrementSizes[1] = m_descriptorIncrementSizes[1] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		m_cmdListRecordContext.m_descriptorIncrementSizes[2] = m_descriptorIncrementSizes[2] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_cmdListRecordContext.m_descriptorIncrementSizes[3] = m_descriptorIncrementSizes[3] = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		m_cmdListRecordContext.m_cpuDescriptorAllocator = &m_cpuDescriptorAllocator;
		m_cmdListRecordContext.m_dsvDescriptorAllocator = &m_cpuDSVDescriptorAllocator;
		m_cmdListRecordContext.m_gpuDescriptorAllocator = &m_gpuDescriptorAllocator;

		m_cmdListRecordContext.m_device = m_device;
	}

	// create command signatures
	{
		// indirect draw
		D3D12_INDIRECT_ARGUMENT_DESC drawIndirectArgumentDesc{};
		drawIndirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

		D3D12_COMMAND_SIGNATURE_DESC drawIndirectCmdSigDesc{};
		drawIndirectCmdSigDesc.ByteStride = sizeof(DrawIndirectCommand);
		drawIndirectCmdSigDesc.NumArgumentDescs = 1;
		drawIndirectCmdSigDesc.pArgumentDescs = &drawIndirectArgumentDesc;
		drawIndirectCmdSigDesc.NodeMask = 0;

		UtilityDx12::checkResult(m_device->CreateCommandSignature(&drawIndirectCmdSigDesc, nullptr, __uuidof(ID3D12CommandSignature), (void **)&m_cmdListRecordContext.m_drawIndirectSignature), "Failed to create command signature!");


		// indirect draw indexed
		D3D12_INDIRECT_ARGUMENT_DESC drawIndexedIndirectArgumentDesc{};
		drawIndexedIndirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC drawIndexedIndirectCmdSigDesc{};
		drawIndexedIndirectCmdSigDesc.ByteStride = sizeof(DrawIndexedIndirectCommand);
		drawIndexedIndirectCmdSigDesc.NumArgumentDescs = 1;
		drawIndexedIndirectCmdSigDesc.pArgumentDescs = &drawIndexedIndirectArgumentDesc;
		drawIndexedIndirectCmdSigDesc.NodeMask = 0;

		UtilityDx12::checkResult(m_device->CreateCommandSignature(&drawIndexedIndirectCmdSigDesc, nullptr, __uuidof(ID3D12CommandSignature), (void **)&m_cmdListRecordContext.m_drawIndexedIndirectSignature), "Failed to create command signature!");


		// indirect dispatch
		D3D12_INDIRECT_ARGUMENT_DESC dispatchIndirectArgumentDesc{};
		dispatchIndirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

		D3D12_COMMAND_SIGNATURE_DESC dispatchIndirectCmdSigDesc{};
		dispatchIndirectCmdSigDesc.ByteStride = sizeof(DispatchIndirectCommand);
		dispatchIndirectCmdSigDesc.NumArgumentDescs = 1;
		dispatchIndirectCmdSigDesc.pArgumentDescs = &dispatchIndirectArgumentDesc;
		dispatchIndirectCmdSigDesc.NodeMask = 0;

		UtilityDx12::checkResult(m_device->CreateCommandSignature(&dispatchIndirectCmdSigDesc, nullptr, __uuidof(ID3D12CommandSignature), (void **)&m_cmdListRecordContext.m_dispatchIndirectSignature), "Failed to create command signature!");
	}

	m_profilingContext = TracyD3D12Context(m_device, (ID3D12CommandQueue *)m_graphicsQueue.getNativeHandle());
}

gal::GraphicsDeviceDx12::~GraphicsDeviceDx12()
{
	waitIdle();

	TracyD3D12Destroy((TracyD3D12Ctx)m_profilingContext);

	//m_gpuMemoryAllocator->destroy();
	//delete m_gpuMemoryAllocator;
	if (m_swapChain)
	{
		delete m_swapChain;
	}

	destroySemaphore(m_graphicsQueue.getWaitIdleSemaphore());
	destroySemaphore(m_computeQueue.getWaitIdleSemaphore());
	destroySemaphore(m_transferQueue.getWaitIdleSemaphore());

	m_cmdListRecordContext.m_drawIndirectSignature->Release();
	m_cmdListRecordContext.m_drawIndexedIndirectSignature->Release();
	m_cmdListRecordContext.m_dispatchIndirectSignature->Release();

	m_cmdListRecordContext.m_gpuDescriptorHeap->Release();
	m_cmdListRecordContext.m_gpuSamplerDescriptorHeap->Release();
	m_cpuDescriptorHeap->Release();
	m_cpuSamplerDescriptorHeap->Release();
	m_cpuRTVDescriptorHeap->Release();
	m_cpuDSVDescriptorHeap->Release();

	((ID3D12CommandQueue *)m_graphicsQueue.getNativeHandle())->Release();
	((ID3D12CommandQueue *)m_computeQueue.getNativeHandle())->Release();
	((ID3D12CommandQueue *)m_transferQueue.getNativeHandle())->Release();

	m_gpuMemoryAllocator->Release();

	m_device->Release();
}

void gal::GraphicsDeviceDx12::createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto *memory = m_graphicsPipelineMemoryPool.alloc();
		assert(memory);
		pipelines[i] = new(memory) GraphicsPipelineDx12(m_device, createInfo[i]);
	}
}

void gal::GraphicsDeviceDx12::createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto *memory = m_computePipelineMemoryPool.alloc();
		assert(memory);
		pipelines[i] = new(memory) ComputePipelineDx12(m_device, createInfo[i]);
	}
}

void gal::GraphicsDeviceDx12::destroyGraphicsPipeline(GraphicsPipeline *pipeline)
{
	if (pipeline)
	{
		auto *pipelineDx = dynamic_cast<GraphicsPipelineDx12 *>(pipeline);
		assert(pipelineDx);

		// call destructor and free backing memory
		pipelineDx->~GraphicsPipelineDx12();
		m_graphicsPipelineMemoryPool.free(reinterpret_cast<RawView<GraphicsPipelineDx12> *>(pipelineDx));
	}
}

void gal::GraphicsDeviceDx12::destroyComputePipeline(ComputePipeline *pipeline)
{
	if (pipeline)
	{
		auto *pipelineDx = dynamic_cast<ComputePipelineDx12 *>(pipeline);
		assert(pipelineDx);

		// call destructor and free backing memory
		pipelineDx->~ComputePipelineDx12();
		m_computePipelineMemoryPool.free(reinterpret_cast<RawView<ComputePipelineDx12> *>(pipelineDx));
	}
}

void gal::GraphicsDeviceDx12::createCommandListPool(const Queue *queue, CommandListPool **commandListPool)
{
	auto *memory = m_commandListPoolMemoryPool.alloc();
	assert(memory);
	auto *queueDx = dynamic_cast<const QueueDx12 *>(queue);
	assert(queueDx);

	const uint32_t queueTypeIdx = static_cast<uint32_t>(queueDx->getQueueType());
	assert(queueTypeIdx < 3);

	D3D12_COMMAND_LIST_TYPE cmdListTypes[]{ D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
	*commandListPool = new(memory) CommandListPoolDx12(m_device, cmdListTypes[queueTypeIdx], &m_cmdListRecordContext);
}

void gal::GraphicsDeviceDx12::destroyCommandListPool(CommandListPool *commandListPool)
{
	if (commandListPool)
	{
		auto *poolDx = dynamic_cast<CommandListPoolDx12 *>(commandListPool);
		assert(poolDx);

		// call destructor and free backing memory
		poolDx->~CommandListPoolDx12();
		m_commandListPoolMemoryPool.free(reinterpret_cast<RawView<CommandListPoolDx12> *>(poolDx));
	}
}

void gal::GraphicsDeviceDx12::createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool)
{
	auto *memory = m_queryPoolMemoryPool.alloc();
	assert(memory);
	*queryPool = new(memory) QueryPoolDx12(m_device, queryType, queryCount, (QueryPipelineStatisticFlags)0);
}

void gal::GraphicsDeviceDx12::destroyQueryPool(QueryPool *queryPool)
{
	if (queryPool)
	{
		auto *poolDx = dynamic_cast<QueryPoolDx12 *>(queryPool);
		assert(poolDx);

		// call destructor and free backing memory
		poolDx->~QueryPoolDx12();
		m_queryPoolMemoryPool.free(reinterpret_cast<RawView<QueryPoolDx12> *>(poolDx));
	}
}

static D3D12_HEAP_PROPERTIES getHeapProperties(MemoryPropertyFlags memoryPropertyFlags)
{
	D3D12_HEAP_PROPERTIES heapProperties{};
	{
		if ((memoryPropertyFlags & MemoryPropertyFlags::DEVICE_LOCAL_BIT) != 0)
		{
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L1;
		}
		else if ((memoryPropertyFlags & MemoryPropertyFlags::HOST_VISIBLE_BIT) != 0 && (memoryPropertyFlags & MemoryPropertyFlags::HOST_COHERENT_BIT) != 0)
		{
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		}
		else if ((memoryPropertyFlags & MemoryPropertyFlags::HOST_VISIBLE_BIT) != 0 && (memoryPropertyFlags & MemoryPropertyFlags::HOST_CACHED_BIT) != 0)
		{
			heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		}
		else
		{
			util::fatalExit("Unsupported combination of MemoryPropertyFlags!", EXIT_FAILURE);
		}

		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.CreationNodeMask = 0;
		heapProperties.VisibleNodeMask = 0;
	}

	return heapProperties;
}

void gal::GraphicsDeviceDx12::createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image)
{
	auto *memory = m_imageMemoryPool.alloc();
	assert(memory);

	D3D12_RESOURCE_DESC resourceDesc{};
	{
		switch (imageCreateInfo.m_imageType)
		{
		case ImageType::_1D:
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			break;
		case ImageType::_2D:
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			break;
		case ImageType::_3D:
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			break;
		default:
			assert(false);
			break;
		}

		bool compressedFormat = Initializers::getFormatInfo(imageCreateInfo.m_format).m_compressed;

		resourceDesc.Alignment = 0;
		resourceDesc.Width = compressedFormat ? util::alignUp(imageCreateInfo.m_width, 4u) : imageCreateInfo.m_width;
		resourceDesc.Height = compressedFormat ? util::alignUp(imageCreateInfo.m_height, 4u) : imageCreateInfo.m_height;
		resourceDesc.DepthOrArraySize = (imageCreateInfo.m_imageType == ImageType::_3D) ? imageCreateInfo.m_depth : imageCreateInfo.m_layers;
		resourceDesc.MipLevels = imageCreateInfo.m_levels;
		resourceDesc.Format = UtilityDx12::translate(imageCreateInfo.m_format);
		resourceDesc.SampleDesc.Count = static_cast<UINT>(imageCreateInfo.m_samples);
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = UtilityDx12::translateImageUsageFlags(imageCreateInfo.m_usageFlags);

		// if the image is used both as depth buffer and as texture, we need to set its format to TYPELESS and use typed formats for the DSV and SRV
		if ((imageCreateInfo.m_usageFlags & ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT & ImageUsageFlags::TEXTURE_BIT) != 0)
		{
			resourceDesc.Format = UtilityDx12::getTypeless(resourceDesc.Format);
		}
	}

	bool useClearValue = false;
	D3D12_CLEAR_VALUE optimizedClearValue{};
	optimizedClearValue.Format = resourceDesc.Format;
	if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
	{
		optimizedClearValue.Color[0] = imageCreateInfo.m_optimizedClearValue.m_color.m_float32[0];
		optimizedClearValue.Color[1] = imageCreateInfo.m_optimizedClearValue.m_color.m_float32[1];
		optimizedClearValue.Color[2] = imageCreateInfo.m_optimizedClearValue.m_color.m_float32[2];
		optimizedClearValue.Color[3] = imageCreateInfo.m_optimizedClearValue.m_color.m_float32[3];
		useClearValue = true;
	}
	else if ((resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
	{
		optimizedClearValue.DepthStencil.Depth = imageCreateInfo.m_optimizedClearValue.m_depthStencil.m_depth;
		optimizedClearValue.DepthStencil.Stencil = imageCreateInfo.m_optimizedClearValue.m_depthStencil.m_stencil;
		useClearValue = true;
	}

	D3D12MA::ALLOCATION_DESC allocationDesc{};
	allocationDesc.Flags = dedicated ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;
	allocationDesc.HeapType = getHeapProperties(requiredMemoryPropertyFlags).Type;

	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
	if (allocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD)
	{
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	else if (allocationDesc.HeapType == D3D12_HEAP_TYPE_READBACK)
	{
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	ID3D12Resource *nativeHandle = nullptr;
	D3D12MA::Allocation *allocHandle = nullptr;

	UtilityDx12::checkResult(m_gpuMemoryAllocator->CreateResource(&allocationDesc, &resourceDesc, initialState, useClearValue ? &optimizedClearValue : nullptr, &allocHandle, __uuidof(ID3D12Resource), (void **)&nativeHandle), "Failed to create resource!");

	*image = new(memory) ImageDx12(nativeHandle, allocHandle, imageCreateInfo, allocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD, allocationDesc.HeapType == D3D12_HEAP_TYPE_READBACK, dedicated);
}

void gal::GraphicsDeviceDx12::createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer)
{
	auto *memory = m_bufferMemoryPool.alloc();
	assert(memory);

	D3D12_RESOURCE_DESC resourceDesc{};
	{
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = util::alignUp((UINT64)bufferCreateInfo.m_size, (UINT64)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = UtilityDx12::translateBufferUsageFlags(bufferCreateInfo.m_usageFlags);
	}

	D3D12MA::ALLOCATION_DESC allocationDesc{};
	allocationDesc.Flags = dedicated ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;
	allocationDesc.HeapType = getHeapProperties(requiredMemoryPropertyFlags).Type;

	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
	if (allocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD)
	{
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	else if (allocationDesc.HeapType == D3D12_HEAP_TYPE_READBACK)
	{
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	ID3D12Resource *nativeHandle = nullptr;
	D3D12MA::Allocation *allocHandle = nullptr;

	UtilityDx12::checkResult(m_gpuMemoryAllocator->CreateResource(&allocationDesc, &resourceDesc, initialState, nullptr, &allocHandle, __uuidof(ID3D12Resource), (void **)&nativeHandle), "Failed to create resource!");

	*buffer = new(memory) BufferDx12(nativeHandle, allocHandle, bufferCreateInfo, allocationDesc.HeapType == D3D12_HEAP_TYPE_UPLOAD, allocationDesc.HeapType == D3D12_HEAP_TYPE_READBACK);
}

void gal::GraphicsDeviceDx12::destroyImage(Image *image)
{
	if (image)
	{
		auto *imageDx = dynamic_cast<ImageDx12 *>(image);
		assert(imageDx);

		((D3D12MA::Allocation *)imageDx->getAllocationHandle())->Release();
		((ID3D12Resource *)imageDx->getNativeHandle())->Release();

		// call destructor and free backing memory
		imageDx->~ImageDx12();
		m_imageMemoryPool.free(reinterpret_cast<RawView<ImageDx12> *>(imageDx));
	}
}

void gal::GraphicsDeviceDx12::destroyBuffer(Buffer *buffer)
{
	if (buffer)
	{
		auto *bufferDx = dynamic_cast<BufferDx12 *>(buffer);
		assert(bufferDx);

		((D3D12MA::Allocation *)bufferDx->getAllocationHandle())->Release();
		((ID3D12Resource *)bufferDx->getNativeHandle())->Release();

		// call destructor and free backing memory
		bufferDx->~BufferDx12();
		m_bufferMemoryPool.free(reinterpret_cast<RawView<BufferDx12> *>(bufferDx));
	}
}

void gal::GraphicsDeviceDx12::createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView)
{
	auto *memory = m_imageViewMemoryPool.alloc();
	assert(memory);

	*imageView = new(memory) ImageViewDx12(m_device, imageViewCreateInfo,
		m_cpuDescriptorAllocator, m_cpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorIncrementSizes[0],
		m_cpuRTVDescriptorAllocator, m_cpuRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorIncrementSizes[2],
		m_cpuDSVDescriptorAllocator, m_cpuDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorIncrementSizes[3]);
}

void gal::GraphicsDeviceDx12::createImageView(Image *image, ImageView **imageView)
{
	const auto &imageDesc = image->getDescription();

	ImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.m_image = image;
	imageViewCreateInfo.m_viewType = static_cast<ImageViewType>(imageDesc.m_imageType);
	imageViewCreateInfo.m_format = imageDesc.m_format;
	imageViewCreateInfo.m_components = {};
	imageViewCreateInfo.m_baseMipLevel = 0;
	imageViewCreateInfo.m_levelCount = imageDesc.m_levels;
	imageViewCreateInfo.m_baseArrayLayer = 0;
	imageViewCreateInfo.m_layerCount = imageDesc.m_layers;

	createImageView(imageViewCreateInfo, imageView);
}

void gal::GraphicsDeviceDx12::createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView)
{
	auto *memory = m_bufferViewMemoryPool.alloc();
	assert(memory);

	*bufferView = new(memory) BufferViewDx12(m_device, bufferViewCreateInfo, m_cpuDescriptorAllocator, m_cpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorIncrementSizes[0]);
}

void gal::GraphicsDeviceDx12::destroyImageView(ImageView *imageView)
{
	if (imageView)
	{
		auto *viewDx = dynamic_cast<ImageViewDx12 *>(imageView);
		assert(viewDx);

		// call destructor and free backing memory
		viewDx->~ImageViewDx12();
		m_imageViewMemoryPool.free(reinterpret_cast<RawView<ImageViewDx12> *>(viewDx));
	}
}

void gal::GraphicsDeviceDx12::destroyBufferView(BufferView *bufferView)
{
	if (bufferView)
	{
		auto *viewDx = dynamic_cast<BufferViewDx12 *>(bufferView);
		assert(viewDx);

		// call destructor and free backing memory
		viewDx->~BufferViewDx12();
		m_bufferViewMemoryPool.free(reinterpret_cast<RawView<BufferViewDx12> *>(viewDx));
	}
}

void gal::GraphicsDeviceDx12::createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler)
{
	auto *memory = m_samplerMemoryPool.alloc();
	assert(memory);

	*sampler = new(memory) SamplerDx12(m_device, samplerCreateInfo, m_cpuSamplerDescriptorAllocator, m_cpuSamplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_descriptorIncrementSizes[1]);
}

void gal::GraphicsDeviceDx12::destroySampler(Sampler *sampler)
{
	if (sampler)
	{
		auto *samplerDx = dynamic_cast<SamplerDx12 *>(sampler);
		assert(samplerDx);

		// call destructor and free backing memory
		samplerDx->~SamplerDx12();
		m_samplerMemoryPool.free(reinterpret_cast<RawView<SamplerDx12> *>(samplerDx));
	}
}

void gal::GraphicsDeviceDx12::createSemaphore(uint64_t initialValue, Semaphore **semaphore)
{
	auto *memory = m_semaphoreMemoryPool.alloc();
	assert(memory);

	*semaphore = new(memory) SemaphoreDx12(m_device, initialValue);
}

void gal::GraphicsDeviceDx12::destroySemaphore(Semaphore *semaphore)
{
	if (semaphore)
	{
		auto *semaphoreDx = dynamic_cast<SemaphoreDx12 *>(semaphore);
		assert(semaphoreDx);

		// call destructor and free backing memory
		semaphoreDx->~SemaphoreDx12();
		m_semaphoreMemoryPool.free(reinterpret_cast<RawView<SemaphoreDx12> *>(semaphoreDx));
	}
}

void gal::GraphicsDeviceDx12::createDescriptorSetPool(uint32_t maxSets, const DescriptorSetLayout *descriptorSetLayout, DescriptorSetPool **descriptorSetPool)
{
	auto *memory = m_descriptorSetPoolMemoryPool.alloc();
	assert(memory);

	const DescriptorSetLayoutDx12 *layoutDx = dynamic_cast<const DescriptorSetLayoutDx12 *>(descriptorSetLayout);
	assert(layoutDx);

	bool samplerPool = layoutDx->needsSamplerHeap();

	TLSFAllocator &heapAllocator = samplerPool ? m_gpuSamplerDescriptorAllocator : m_gpuDescriptorAllocator;
	ID3D12DescriptorHeap *heap = samplerPool ? m_cmdListRecordContext.m_gpuSamplerDescriptorHeap : m_cmdListRecordContext.m_gpuDescriptorHeap;
	UINT incSize = samplerPool ? m_descriptorIncrementSizes[1] : m_descriptorIncrementSizes[0];

	*descriptorSetPool = new(memory) DescriptorSetPoolDx12(m_device, heapAllocator, heap->GetCPUDescriptorHandleForHeapStart(), heap->GetGPUDescriptorHandleForHeapStart(), incSize, layoutDx, maxSets);
}

void gal::GraphicsDeviceDx12::destroyDescriptorSetPool(DescriptorSetPool *descriptorSetPool)
{
	if (descriptorSetPool)
	{
		auto *poolDx = dynamic_cast<DescriptorSetPoolDx12 *>(descriptorSetPool);
		assert(poolDx);

		// call destructor and free backing memory
		poolDx->~DescriptorSetPoolDx12();
		m_descriptorSetPoolMemoryPool.free(reinterpret_cast<RawView<DescriptorSetPoolDx12> *>(poolDx));
	}
}

void gal::GraphicsDeviceDx12::createDescriptorSetLayout(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings, DescriptorSetLayout **descriptorSetLayout)
{
	auto *memory = m_descriptorSetLayoutMemoryPool.alloc();
	assert(memory);

	*descriptorSetLayout = new(memory) DescriptorSetLayoutDx12(bindingCount, bindings);
}

void gal::GraphicsDeviceDx12::destroyDescriptorSetLayout(DescriptorSetLayout *descriptorSetLayout)
{
	if (descriptorSetLayout)
	{
		auto *layoutDx = dynamic_cast<DescriptorSetLayoutDx12 *>(descriptorSetLayout);
		assert(layoutDx);

		// call destructor and free backing memory
		layoutDx->~DescriptorSetLayoutDx12();
		m_descriptorSetLayoutMemoryPool.free(reinterpret_cast<RawView<DescriptorSetLayoutDx12> *>(layoutDx));
	}
}

void gal::GraphicsDeviceDx12::createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode, SwapChain **swapChain)
{
	assert(!m_swapChain);
	assert(width && height);
	Queue *queue = nullptr;
	queue = presentQueue == &m_graphicsQueue ? &m_graphicsQueue : queue;
	queue = presentQueue == &m_computeQueue ? &m_computeQueue : queue;
	assert(queue);
	*swapChain = m_swapChain = new SwapChainDx12(this, m_device, m_windowHandle, queue, width, height, fullscreen, presentMode);
}

void gal::GraphicsDeviceDx12::destroySwapChain()
{
	assert(m_swapChain);
	delete m_swapChain;
	m_swapChain = nullptr;
}

void gal::GraphicsDeviceDx12::waitIdle()
{
	m_graphicsQueue.waitIdle();
	m_computeQueue.waitIdle();
	m_transferQueue.waitIdle();
}

void gal::GraphicsDeviceDx12::setDebugObjectName(ObjectType objectType, void *object, const char *name)
{
	constexpr size_t wStrLen = 1024;
	wchar_t wName[wStrLen];

	size_t ret;
	mbstowcs_s(&ret, wName, name, wStrLen - 1);

	switch (objectType)
	{
	case ObjectType::QUEUE:
		((ID3D12CommandQueue *)((QueueDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::SEMAPHORE:
		((ID3D12Fence *)((SemaphoreDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::COMMAND_LIST:
		((ID3D12GraphicsCommandList5 *)((CommandListDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::BUFFER:
		((ID3D12Resource *)((BufferDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::IMAGE:
		((ID3D12Resource *)((ImageDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::QUERY_POOL:
		((ID3D12QueryHeap *)((QueryPoolDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::BUFFER_VIEW:
		break;
	case gal::ObjectType::IMAGE_VIEW:
		break;
	case gal::ObjectType::GRAPHICS_PIPELINE:
		((ID3D12PipelineState *)((GraphicsPipelineDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::COMPUTE_PIPELINE:
		((ID3D12PipelineState *)((ComputePipelineDx12 *)object)->getNativeHandle())->SetName(wName);
		break;
	case gal::ObjectType::DESCRIPTOR_SET_LAYOUT:
		break;
	case gal::ObjectType::SAMPLER:
		break;
	case gal::ObjectType::DESCRIPTOR_SET_POOL:
		break;
	case gal::ObjectType::DESCRIPTOR_SET:
		break;
	case gal::ObjectType::COMMAND_LIST_POOL:
		break;
	case gal::ObjectType::SWAPCHAIN:
		break;
	default:
		break;
	}
}

gal::Queue *gal::GraphicsDeviceDx12::getGraphicsQueue()
{
	return &m_graphicsQueue;
}

gal::Queue *gal::GraphicsDeviceDx12::getComputeQueue()
{
	return &m_computeQueue;
}

gal::Queue *gal::GraphicsDeviceDx12::getTransferQueue()
{
	return &m_transferQueue;
}

uint64_t gal::GraphicsDeviceDx12::getBufferAlignment(DescriptorType bufferType, uint64_t elementSize) const
{
	switch (bufferType)
	{
	case DescriptorType::TYPED_BUFFER:
		return elementSize;
	case DescriptorType::RW_TYPED_BUFFER:
		return elementSize;
	case DescriptorType::CONSTANT_BUFFER:
		return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	case DescriptorType::BYTE_BUFFER:
		return D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
	case DescriptorType::RW_BYTE_BUFFER:
		return D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
	case DescriptorType::STRUCTURED_BUFFER:
		return elementSize;
	case DescriptorType::RW_STRUCTURED_BUFFER:
		return elementSize;
	case DescriptorType::OFFSET_CONSTANT_BUFFER:
		return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	default:
		assert(false);
		break;
	}
	return uint64_t();
}

uint64_t gal::GraphicsDeviceDx12::getMinUniformBufferOffsetAlignment() const
{
	return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
}

uint64_t gal::GraphicsDeviceDx12::getMinStorageBufferOffsetAlignment() const
{
	return D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
}

uint64_t gal::GraphicsDeviceDx12::getBufferCopyOffsetAlignment() const
{
	return D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
}

uint64_t gal::GraphicsDeviceDx12::getBufferCopyRowPitchAlignment() const
{
	return D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
}

float gal::GraphicsDeviceDx12::getMaxSamplerAnisotropy() const
{
	return D3D12_MAX_MAXANISOTROPY;
}

void *gal::GraphicsDeviceDx12::getProfilingContext() const
{
	return m_profilingContext;
}
