#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"
#include "ResourceVk.h"
#include "Utility/ObjectPool.h"

namespace gal
{
	class SwapChainVk : public SwapChain
	{
	public:
		explicit SwapChainVk(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode);
		SwapChainVk(SwapChainVk &) = delete;
		SwapChainVk(SwapChainVk &&) = delete;
		SwapChainVk &operator=(const SwapChainVk &) = delete;
		SwapChainVk &operator=(const SwapChainVk &&) = delete;
		~SwapChainVk();
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
		static constexpr uint32_t s_maxImageCount = 8;
		static constexpr uint32_t s_semaphoreCount = 3;

		VkPhysicalDevice m_physicalDevice;
		VkDevice m_device;
		VkSurfaceKHR m_surface;
		Queue *m_presentQueue;
		VkSwapchainKHR m_swapChain;
		uint32_t m_imageCount;
		ImageVk *m_images[s_maxImageCount];
		StaticObjectPool<ByteArray<sizeof(ImageVk)>, s_maxImageCount> m_imageMemoryPool;
		Format m_imageFormat;
		Extent2D m_extent;
		uint32_t m_currentImageIndex;
		VkSemaphore m_acquireSemaphores[s_semaphoreCount];
		VkSemaphore m_presentSemaphores[s_semaphoreCount];
		uint64_t m_frameIndex;
		bool m_fullscreen;
		PresentMode m_presentMode;

		void create(uint32_t width, uint32_t height);
		void destroy();
		void resize(uint32_t width, uint32_t height, bool acquireImage);
		uint32_t acquireImageIndex(VkSemaphore semaphore);
	};
}