#include "SwapChainDx12.h"
#include "UtilityDx12.h"
#include "GraphicsDeviceDx12.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

gal::SwapChainDx12::SwapChainDx12(GraphicsDeviceDx12 *graphicsDevice, ID3D12Device *device, void *windowHandle, Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode)
	:m_graphicsDeviceDx12(graphicsDevice),
	m_device(device),
	m_windowHandle(windowHandle),
	m_presentQueue(presentQueue),
	m_swapChain(),
	m_imageCount(),
	m_images(),
	m_imageMemoryPool(),
	m_imageFormat(),
	m_extent(),
	m_currentImageIndex(),
	m_fullscreen(fullscreen),
	m_presentMode(presentMode)
{
	IDXGIFactory4 *factory;
	UtilityDx12::checkResult(CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), (void **)&factory), "Failed to create DXGIFactory!");

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = 0;
	swapChainDesc.Height = 0;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = s_maxImageCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = 0;

	IDXGISwapChain1 *swapChain;
	UtilityDx12::checkResult(factory->CreateSwapChainForHwnd((ID3D12CommandQueue *)m_presentQueue->getNativeHandle(), glfwGetWin32Window((GLFWwindow *)m_windowHandle), &swapChainDesc, nullptr, nullptr, &swapChain), "Failed to create swapchain!");
	UtilityDx12::checkResult(swapChain->QueryInterface(__uuidof(IDXGISwapChain4), (void **)&m_swapChain), "Failed to create swapchain!");

	swapChain->Release();

	UtilityDx12::checkResult(factory->MakeWindowAssociation((HWND)m_windowHandle, DXGI_MWA_NO_ALT_ENTER), "Failed to create swapchain!");

	if (m_fullscreen)
	{
		UtilityDx12::checkResult(m_swapChain->SetFullscreenState(true, nullptr), "Failed to set swapchain to fullscreen!");
		UtilityDx12::checkResult(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "Failed to resize swapchain buffers!");
	}

	m_swapChain->GetDesc1(&swapChainDesc);

	m_extent.m_width = swapChainDesc.Width;
	m_extent.m_height = swapChainDesc.Height;

	m_imageFormat = Format::B8G8R8A8_UNORM;

	m_imageCount = s_maxImageCount;

	ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.m_width = m_extent.m_width;
	imageCreateInfo.m_height = m_extent.m_height;
	imageCreateInfo.m_depth = 1;
	imageCreateInfo.m_layers = 1;
	imageCreateInfo.m_levels = 1;
	imageCreateInfo.m_samples = SampleCount::_1;
	imageCreateInfo.m_imageType = ImageType::_2D;
	imageCreateInfo.m_format = m_imageFormat;
	imageCreateInfo.m_createFlags = {};
	imageCreateInfo.m_usageFlags = ImageUsageFlags::TRANSFER_DST_BIT | ImageUsageFlags::COLOR_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT;

	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		ID3D12Resource *imageDx;
		UtilityDx12::checkResult(m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&imageDx), "Failed to retrieve swapchain image!");
		auto *memory = m_imageMemoryPool.alloc();
		assert(memory);

		m_images[i] = new(memory) ImageDx12(imageDx, nullptr, imageCreateInfo, false, false, true);
	}
}

gal::SwapChainDx12::~SwapChainDx12()
{
	if (m_fullscreen)
	{
		UtilityDx12::checkResult(m_swapChain->SetFullscreenState(false, nullptr), "Failed to set swapchain to windowed!");
	}

	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		((ID3D12Resource *)m_images[i]->getNativeHandle())->Release();
		// call destructor and free backing memory
		m_images[i]->~ImageDx12();
		m_imageMemoryPool.free(reinterpret_cast<RawView<ImageDx12> *>(m_images[i]));
	}
	m_swapChain->Release();
}

void *gal::SwapChainDx12::getNativeHandle() const
{
	return m_swapChain;
}

void gal::SwapChainDx12::resize(uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode)
{
	if (width == m_extent.m_width && height == m_extent.m_height && fullscreen == m_fullscreen)
	{
		m_presentMode = presentMode;
		return;
	}

	m_graphicsDeviceDx12->waitIdle();

	// free swapchain images
	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		((ID3D12Resource *)m_images[i]->getNativeHandle())->Release();
		// call destructor and free backing memory
		m_images[i]->~ImageDx12();
		m_imageMemoryPool.free(reinterpret_cast<RawView<ImageDx12> *>(m_images[i]));
	}

	if (fullscreen != m_fullscreen)
	{
		m_fullscreen = fullscreen;
		UtilityDx12::checkResult(m_swapChain->SetFullscreenState(m_fullscreen, nullptr), "Failed to set swapchain fullscreen state!");
	}

	// resize
	UtilityDx12::checkResult(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "Failed to resize swapchain buffers!");

	m_presentMode = presentMode;

	// update extent
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	m_swapChain->GetDesc1(&swapChainDesc);
	m_extent.m_width = swapChainDesc.Width;
	m_extent.m_height = swapChainDesc.Height;

	// get swapchain images
	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		ID3D12Resource *imageDx;
		UtilityDx12::checkResult(m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&imageDx), "Failed to retrieve swapchain image!");
		auto *memory = m_imageMemoryPool.alloc();
		assert(memory);

		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = m_extent.m_width;
		imageCreateInfo.m_height = m_extent.m_height;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = m_imageFormat;
		imageCreateInfo.m_createFlags = {};
		imageCreateInfo.m_usageFlags = ImageUsageFlags::TRANSFER_DST_BIT | ImageUsageFlags::COLOR_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT;

		m_images[i] = new(memory) ImageDx12(imageDx, nullptr, imageCreateInfo, false, false, true);
	}
}

uint32_t gal::SwapChainDx12::getCurrentImageIndex()
{
	return m_swapChain->GetCurrentBackBufferIndex();
}

void gal::SwapChainDx12::present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue)
{
	//waitSemaphore->wait(semaphoreWaitValue);
	UtilityDx12::checkResult(m_swapChain->Present(m_presentMode == PresentMode::IMMEDIATE ? 0 : 1, 0), "Failed to present!");
	UtilityDx12::checkResult(((ID3D12CommandQueue *)m_presentQueue->getNativeHandle())->Signal((ID3D12Fence *)signalSemaphore->getNativeHandle(), semaphoreSignalValue), "Failed to signal Fence after present!");
}

gal::Extent2D gal::SwapChainDx12::getExtent() const
{
	return m_extent;
}

gal::Extent2D gal::SwapChainDx12::getRecreationExtent() const
{
	return m_extent;
}

gal::Format gal::SwapChainDx12::getImageFormat() const
{
	return m_imageFormat;
}

gal::Image *gal::SwapChainDx12::getImage(size_t index) const
{
	assert(index < m_imageCount);
	return m_images[index];
}

gal::Queue *gal::SwapChainDx12::getPresentQueue() const
{
	return m_presentQueue;
}