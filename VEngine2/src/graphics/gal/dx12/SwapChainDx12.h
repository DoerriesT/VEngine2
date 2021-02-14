#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include "ResourceDx12.h"
#include "Utility/ObjectPool.h"

namespace gal
{
	class GraphicsDeviceDx12;

	class SwapChainDx12 : public SwapChain
	{
	public:
		explicit SwapChainDx12(GraphicsDeviceDx12 *graphicsDevice, ID3D12Device *device, void *windowHandle, Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode);
		SwapChainDx12(SwapChainDx12 &) = delete;
		SwapChainDx12(SwapChainDx12 &&) = delete;
		SwapChainDx12 &operator=(const SwapChainDx12 &) = delete;
		SwapChainDx12 &operator=(const SwapChainDx12 &&) = delete;
		~SwapChainDx12();
		void *getNativeHandle() const override;
		void resize(uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode) override;
		uint32_t getCurrentImageIndex() override;
		void present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue) override;
		Extent2D getExtent() const override;
		Extent2D getRecreationExtent() const override;
		Format getImageFormat() const override;
		Image *getImage(size_t index) const override;
		Queue *getPresentQueue() const override;

	private:
		static constexpr uint32_t s_maxImageCount = 3;

		GraphicsDeviceDx12 *m_graphicsDeviceDx12;
		ID3D12Device *m_device;
		void *m_windowHandle;
		Queue *m_presentQueue;
		IDXGISwapChain4 *m_swapChain;
		uint32_t m_imageCount;
		ImageDx12 *m_images[s_maxImageCount];
		StaticObjectPool<ByteArray<sizeof(ImageDx12)>, s_maxImageCount> m_imageMemoryPool;
		Format m_imageFormat;
		Extent2D m_extent;
		uint32_t m_currentImageIndex;
		bool m_fullscreen;
		PresentMode m_presentMode;
	};
}