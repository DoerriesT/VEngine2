#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"
#include "ResourceVk.h"
#include "utility/ObjectPool.h"

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
		static constexpr uint32_t k_maxImageCount = 8;
		static constexpr uint32_t k_semaphoreCount = 3;

		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		VkDevice m_device = VK_NULL_HANDLE;
		VkSurfaceKHR m_surface = VK_NULL_HANDLE;
		Queue *m_presentQueue = nullptr;
		VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
		uint32_t m_imageCount = 0;
		ImageVk *m_images[k_maxImageCount] = {};
		StaticObjectMemoryPool<ImageVk, k_maxImageCount> m_imageMemoryPool;
		Format m_imageFormat = Format::UNDEFINED;
		Extent2D m_extent = {};
		uint32_t m_currentImageIndex = -1;
		VkSemaphore m_acquireSemaphores[k_semaphoreCount] = {};
		VkSemaphore m_presentSemaphores[k_semaphoreCount] = {};
		uint64_t m_frameIndex = 0;
		bool m_fullscreen = false;
		PresentMode m_presentMode = PresentMode::IMMEDIATE;

		void create(uint32_t width, uint32_t height);
		void destroy();
		void resize(uint32_t width, uint32_t height, bool acquireImage);
		uint32_t acquireImageIndex(VkSemaphore semaphore);
	};
}