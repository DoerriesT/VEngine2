#include "SwapChainVk.h"
#include "UtilityVk.h"
#include "SemaphoreVk.h"
#include "QueueVk.h"
#include "utility/Utility.h"
#include "utility/Memory.h"

gal::SwapChainVk::SwapChainVk(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode)
	:m_physicalDevice(physicalDevice),
	m_device(device),
	m_surface(surface),
	m_presentQueue(presentQueue),
	m_fullscreen(fullscreen),
	m_presentMode(presentMode)
{
	create(width, height);
}

gal::SwapChainVk::~SwapChainVk()
{
	destroy();
}

void *gal::SwapChainVk::getNativeHandle() const
{
	return m_swapChain;
}

void gal::SwapChainVk::resize(uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode)
{
	m_fullscreen = fullscreen;
	m_presentMode = presentMode;
	resize(width, height, true);
}

uint32_t gal::SwapChainVk::getCurrentImageIndex()
{
	// no present has occured yet, so we dont have a valid m_currentImageIndex.
	// try to acquire an image and wait on a fence, so we can immediately start using the image
	if (m_frameIndex == 0 && m_currentImageIndex == -1)
	{
		m_currentImageIndex = acquireImageIndex(VK_NULL_HANDLE);
	}

	return m_currentImageIndex;
}

void gal::SwapChainVk::present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue)
{
	const uint32_t resIdx = m_frameIndex % k_semaphoreCount;
	VkQueue presentQueueVk = (VkQueue)m_presentQueue->getNativeHandle();

	// Vulkan swapchains do not support timeline semaphores, so we need to wait on the timeline semaphore and signal the binary semaphore
	{
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSemaphore waitSemaphoreVk = (VkSemaphore)waitSemaphore->getNativeHandle();

		uint64_t dummyValue = 0;

		VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timelineSubmitInfo.waitSemaphoreValueCount = 1;
		timelineSubmitInfo.pWaitSemaphoreValues = &semaphoreWaitValue;
		timelineSubmitInfo.signalSemaphoreValueCount = 1;
		timelineSubmitInfo.pSignalSemaphoreValues = &dummyValue;

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO, &timelineSubmitInfo };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphoreVk;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 0;
		submitInfo.pCommandBuffers = nullptr;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_presentSemaphores[resIdx];

		UtilityVk::checkResult(vkQueueSubmit(presentQueueVk, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit to Queue!");
	}

	// present
	{
		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_presentSemaphores[resIdx];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapChain;
		presentInfo.pImageIndices = &m_currentImageIndex;

		UtilityVk::checkResult(vkQueuePresentKHR(presentQueueVk, &presentInfo), "Failed to present!");
	}

	// try to acquire the next image index.
	{
		m_currentImageIndex = acquireImageIndex(m_acquireSemaphores[resIdx]);
	}

	// Vulkan swapchains do not support timeline semaphores, so we need to wait on a binary semaphore and signal the timeline semaphore
	{
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSemaphore signalSemaphoreVk = (VkSemaphore)signalSemaphore->getNativeHandle();

		uint64_t dummyValue = 0;

		VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timelineSubmitInfo.waitSemaphoreValueCount = 1;
		timelineSubmitInfo.pWaitSemaphoreValues = &dummyValue;
		timelineSubmitInfo.signalSemaphoreValueCount = 1;
		timelineSubmitInfo.pSignalSemaphoreValues = &semaphoreSignalValue;

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.pNext = &timelineSubmitInfo;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_acquireSemaphores[resIdx];
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 0;
		submitInfo.pCommandBuffers = nullptr;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphoreVk;

		UtilityVk::checkResult(vkQueueSubmit(presentQueueVk, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit to Queue!");
	}

	++m_frameIndex;
}

gal::Extent2D gal::SwapChainVk::getExtent() const
{
	return m_extent;
}

gal::Extent2D gal::SwapChainVk::getRecreationExtent() const
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

	const auto extentVk = surfaceCapabilities.currentExtent.width != UINT32_MAX ? surfaceCapabilities.currentExtent : surfaceCapabilities.minImageExtent;
	return { extentVk.width, extentVk.height };
}

gal::Format gal::SwapChainVk::getImageFormat() const
{
	return m_imageFormat;
}

gal::Image *gal::SwapChainVk::getImage(size_t index) const
{
	assert(index < m_imageCount);
	return m_images[index];
}

gal::Queue *gal::SwapChainVk::getPresentQueue() const
{
	return m_presentQueue;
}

void gal::SwapChainVk::create(uint32_t width, uint32_t height)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
	VkSurfaceFormatKHR *formats = ALLOC_A_T(VkSurfaceFormatKHR, formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats);

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
	VkPresentModeKHR *presentModes = ALLOC_A_T(VkPresentModeKHR, presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes);

	// find surface format
	VkSurfaceFormatKHR surfaceFormat;
	{
		bool foundOptimal = false;
		for (size_t i = 0; i < formatCount; ++i)
		{
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = formats[i];
				foundOptimal = true;
				break;
			}
		}
		if (!foundOptimal)
		{
			surfaceFormat = formats[0];
		}
	}

	// find present mode
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // VK_PRESENT_MODE_FIFO_KHR is always supported
	{
		for (size_t i = 0; i < presentModeCount; ++i)
		{
			// prefer VK_PRESENT_MODE_MAILBOX_KHR to VK_PRESENT_MODE_FIFO_KHR for vsync
			if (m_presentMode == PresentMode::V_SYNC && presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = presentModes[i];
				break;
			}
			else if (m_presentMode == PresentMode::IMMEDIATE && presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				presentMode = presentModes[i];
				break;
			}
		}
	}

	// find proper extent
	VkExtent2D extent;
	{
		if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
		{
			extent = surfaceCapabilities.currentExtent;
		}
		else
		{
			extent =
			{
				eastl::max(surfaceCapabilities.minImageExtent.width, eastl::min(surfaceCapabilities.maxImageExtent.width, width)),
				eastl::max(surfaceCapabilities.minImageExtent.height, eastl::min(surfaceCapabilities.maxImageExtent.height, height))
			};
		}
	}

	m_imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && m_imageCount > surfaceCapabilities.maxImageCount)
	{
		m_imageCount = surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = m_surface;
	createInfo.minImageCount = m_imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = surfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create swap chain!", EXIT_FAILURE);
	}

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_imageCount, nullptr);
	if (m_imageCount > k_maxImageCount)
	{
		util::fatalExit("Swap chain image count higher than supported maximum!", EXIT_FAILURE);
	}

	VkImage imagesVk[k_maxImageCount];

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_imageCount, imagesVk);

	ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.m_width = extent.width;
	imageCreateInfo.m_height = extent.height;
	imageCreateInfo.m_depth = 1;
	imageCreateInfo.m_layers = 1;
	imageCreateInfo.m_levels = 1;
	imageCreateInfo.m_samples = SampleCount::_1;
	imageCreateInfo.m_imageType = ImageType::_2D;
	// TODO: this cast will fail once we modify the gal::Format enum, so create a VkFormat -> gal::Format function
	imageCreateInfo.m_format = static_cast<Format>(surfaceFormat.format);
	imageCreateInfo.m_createFlags = {};
	imageCreateInfo.m_usageFlags = gal::ImageUsageFlags::TRANSFER_DST_BIT | gal::ImageUsageFlags::RW_TEXTURE_BIT | gal::ImageUsageFlags::COLOR_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT;

	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		auto *memory = m_imageMemoryPool.alloc();
		assert(memory);

		m_images[i] = new(memory) ImageVk(imagesVk[i], nullptr, imageCreateInfo);
	}

	// TODO: this cast will fail once we modify the gal::Format enum, so create a VkFormat -> gal::Format function
	m_imageFormat = static_cast<Format>(surfaceFormat.format);
	m_extent = { extent.width, extent.height };

	// create semaphores
	for (size_t i = 0; i < k_semaphoreCount; ++i)
	{
		VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		UtilityVk::checkResult(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_acquireSemaphores[i]), "Failed to create semaphore!");
		UtilityVk::checkResult(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_presentSemaphores[i]), "Failed to create semaphore!");
	}
}

void gal::SwapChainVk::destroy()
{
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	for (size_t i = 0; i < k_semaphoreCount; ++i)
	{
		vkDestroySemaphore(m_device, m_acquireSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_presentSemaphores[i], nullptr);
	}

	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		// call destructor and free backing memory
		m_images[i]->~ImageVk();
		m_imageMemoryPool.free(reinterpret_cast<RawView<ImageVk> *>(m_images[i]));
	}
}

void gal::SwapChainVk::resize(uint32_t width, uint32_t height, bool acquireImage)
{
	vkDeviceWaitIdle(m_device);
	destroy();
	create(width, height);
	if (acquireImage)
	{
		m_currentImageIndex = acquireImageIndex(VK_NULL_HANDLE);
	}
}

uint32_t gal::SwapChainVk::acquireImageIndex(VkSemaphore semaphore)
{
	uint32_t imageIndex;

	VkFence fence = VK_NULL_HANDLE;
	if (semaphore == VK_NULL_HANDLE)
	{
		VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		UtilityVk::checkResult(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence), "Failed to create Fence!");
	}

	bool tryAgain = false;
	int remainingAttempts = 3;
	do
	{
		--remainingAttempts;
		VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, semaphore, fence, &imageIndex);

		switch (result)
		{
		case VK_SUCCESS:
			if (semaphore == VK_NULL_HANDLE)
			{
				UtilityVk::checkResult(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX), "Failed to wait on Fence!");
			}
			break;
		case VK_SUBOPTIMAL_KHR:
			if (semaphore == VK_NULL_HANDLE)
			{
				UtilityVk::checkResult(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX), "Failed to wait on Fence!");
			}
		case VK_ERROR_OUT_OF_DATE_KHR:
			resize(m_extent.m_width, m_extent.m_height, false);
			tryAgain = true;
			break;
		case VK_ERROR_SURFACE_LOST_KHR:
			util::fatalExit("Failed to acquire swap chain image! VK_ERROR_SURFACE_LOST_KHR", EXIT_FAILURE);
			break;

		default:
			break;
		}
	} while (tryAgain && remainingAttempts > 0);

	if (semaphore == VK_NULL_HANDLE)
	{
		// destroy fence, we no longer need it
		vkDestroyFence(m_device, fence, nullptr);
	}

	if (remainingAttempts <= 0)
	{
		util::fatalExit("Failed to acquire swap chain image! Too many failed attempts at swapchain recreation.", EXIT_FAILURE);
	}

	return imageIndex;
}
