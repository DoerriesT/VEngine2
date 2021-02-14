#include "GraphicsDeviceVk.h"
#include "volk.h"
#include <GLFW/glfw3.h>
#include "Utility/Utility.h"
#include <iostream>
#include <set>
#include "RenderPassCacheVk.h"
#include "FramebufferCacheVk.h"
#include "MemoryAllocatorVk.h"
#include "UtilityVk.h"
#include "SwapChainVk.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData)
{
	if (pCallbackData->messageIdNumber == -1498317601)
	{
		return VK_FALSE;
	}
	std::cerr << "validation layer:\n"
		<< "Message ID Name: " << pCallbackData->pMessageIdName << "\n"
		<< "Message ID Number: " << pCallbackData->messageIdNumber << "\n"
		<< "Message: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

gal::GraphicsDeviceVk::GraphicsDeviceVk(void *windowHandle, bool debugLayer)
	:m_instance(),
	m_device(),
	m_physicalDevice(),
	m_features(),
	m_enabledFeatures(),
	m_properties(),
	m_subgroupProperties(),
	m_graphicsQueue(),
	m_computeQueue(),
	m_transferQueue(),
	m_surface(),
	m_debugUtilsMessenger(),
	m_renderPassCache(),
	m_gpuMemoryAllocator(),
	m_swapChain(),
	m_graphicsPipelineMemoryPool(64),
	m_computePipelineMemoryPool(64),
	m_commandListPoolMemoryPool(32),
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
	if (volkInitialize() != VK_SUCCESS)
	{
		util::fatalExit("Failed to initialize volk!", EXIT_FAILURE);
	}

	// create instance
	{
		VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.pApplicationName = "Vulkan";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		// extensions
		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char *> extensions(glfwExtensions, glfwExtensions + static_cast<size_t>(glfwExtensionCount));

		//if (g_vulkanDebugCallBackEnabled)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		// make sure all requested extensions are available
		{
			uint32_t instanceExtensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
			std::vector<VkExtensionProperties> extensionProperties(instanceExtensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, extensionProperties.data());

			for (auto &requestedExtension : extensions)
			{
				bool found = false;
				for (auto &presentExtension : extensionProperties)
				{
					if (strcmp(requestedExtension, presentExtension.extensionName) == 0)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					util::fatalExit(("Requested extension not present! " + std::string(requestedExtension)).c_str(), EXIT_FAILURE);
				}
			}
		}

		VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create instance!", EXIT_FAILURE);
		}
	}

	volkLoadInstance(m_instance);

	// create debug callback
	//if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;

		if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugUtilsMessenger) != VK_SUCCESS)
		{
			util::fatalExit("Failed to set up debug callback!", EXIT_FAILURE);
		}
	}

	// create window surface
	{
		if (glfwCreateWindowSurface(m_instance, (GLFWwindow *)windowHandle, nullptr, &m_surface) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create window surface!", EXIT_FAILURE);
		}
	}

	const char *const deviceExtensions[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const char *const optionalExtensions[]{ VK_EXT_MEMORY_BUDGET_EXTENSION_NAME };
	bool supportsMemoryBudgetExtension = false;

	// pick physical device
	{
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);

		if (physicalDeviceCount == 0)
		{
			util::fatalExit("Failed to find GPUs with Vulkan support!", EXIT_FAILURE);
		}

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());

		struct DeviceInfo
		{
			VkPhysicalDevice m_physicalDevice;
			VkPhysicalDeviceProperties m_properties;
			VkPhysicalDeviceFeatures m_features;
			VkPhysicalDeviceVulkan12Features m_vulkan12Features;
			uint32_t m_graphicsQueueFamily;
			uint32_t m_computeQueueFamily;
			uint32_t m_transferQueueFamily;
			bool m_computeQueuePresentable;
			bool m_memoryBudgetExtensionSupported;
		};

		std::vector<DeviceInfo> suitableDevices;

		for (const auto &physicalDevice : physicalDevices)
		{
			// find queue indices
			int graphicsFamilyIndex = -1;
			int computeFamilyIndex = -1;
			int transferFamilyIndex = -1;
			VkBool32 graphicsFamilyPresentable = VK_FALSE;
			VkBool32 computeFamilyPresentable = VK_FALSE;
			{
				uint32_t queueFamilyCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

				std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
				vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

				// find graphics queue
				for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
				{
					auto &queueFamily = queueFamilies[queueFamilyIndex];
					if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
					{
						graphicsFamilyIndex = queueFamilyIndex;
					}
				}

				// find compute queue
				for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
				{
					// we're trying to find dedicated queues, so skip the graphics queue
					if (queueFamilyIndex == graphicsFamilyIndex)
					{
						continue;
					}

					auto &queueFamily = queueFamilies[queueFamilyIndex];
					if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
					{
						computeFamilyIndex = queueFamilyIndex;
					}
				}

				// find transfer queue
				for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
				{
					// we're trying to find dedicated queues, so skip the graphics/compute queue
					if (queueFamilyIndex == graphicsFamilyIndex || queueFamilyIndex == computeFamilyIndex)
					{
						continue;
					}

					auto &queueFamily = queueFamilies[queueFamilyIndex];
					if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
					{
						transferFamilyIndex = queueFamilyIndex;
					}
				}

				// use graphics queue if no dedicated other queues
				if (computeFamilyIndex == -1)
				{
					computeFamilyIndex = graphicsFamilyIndex;
				}
				if (transferFamilyIndex == -1)
				{
					transferFamilyIndex = graphicsFamilyIndex;
				}

				// query present support
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsFamilyIndex, m_surface, &graphicsFamilyPresentable);
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, computeFamilyIndex, m_surface, &computeFamilyPresentable);
			}

			// test if all required extensions are supported by this physical device
			bool extensionsSupported = false;
			bool memoryBudgetExtensionSupported = false;
			{
				uint32_t extensionCount;
				vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

				std::vector<VkExtensionProperties> availableExtensions(extensionCount);
				vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

				std::set<std::string> requiredExtensions(std::begin(deviceExtensions), std::end(deviceExtensions));

				for (const auto &extension : availableExtensions)
				{
					requiredExtensions.erase(extension.extensionName);
					if (strcmp(extension.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0)
					{
						memoryBudgetExtensionSupported = true;
					}
				}

				extensionsSupported = requiredExtensions.empty();
			}

			// test if the device supports a swapchain
			bool swapChainAdequate = false;
			if (extensionsSupported)
			{
				uint32_t formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);

				swapChainAdequate = formatCount != 0 && presentModeCount != 0;
			}

			VkPhysicalDeviceVulkan12Features vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			VkPhysicalDeviceFeatures2 supportedFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &vulkan12Features };

			vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures2);

			VkPhysicalDeviceFeatures supportedFeatures = supportedFeatures2.features;

			if (graphicsFamilyIndex >= 0
				&& computeFamilyIndex >= 0
				&& transferFamilyIndex >= 0
				&& graphicsFamilyPresentable
				//&& computeFamilyPresentable
				&& extensionsSupported
				&& swapChainAdequate
				&& supportedFeatures.samplerAnisotropy
				&& supportedFeatures.textureCompressionBC
				&& supportedFeatures.independentBlend
				&& supportedFeatures.fragmentStoresAndAtomics
				&& supportedFeatures.shaderStorageImageExtendedFormats
				&& supportedFeatures.shaderStorageImageWriteWithoutFormat
				&& supportedFeatures.imageCubeArray
				&& supportedFeatures.geometryShader
				&& vulkan12Features.shaderSampledImageArrayNonUniformIndexing
				&& vulkan12Features.timelineSemaphore
				&& vulkan12Features.imagelessFramebuffer
				&& vulkan12Features.descriptorBindingPartiallyBound
				//&& vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind
				&& vulkan12Features.descriptorBindingSampledImageUpdateAfterBind
				&& vulkan12Features.descriptorBindingStorageImageUpdateAfterBind
				&& vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind
				&& vulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind
				&& vulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind
				&& vulkan12Features.descriptorBindingUpdateUnusedWhilePending
				&& vulkan12Features.descriptorBindingPartiallyBound)
			{
				DeviceInfo deviceInfo{};
				deviceInfo.m_physicalDevice = physicalDevice;
				vkGetPhysicalDeviceProperties(physicalDevice, &deviceInfo.m_properties);
				deviceInfo.m_features = supportedFeatures;
				deviceInfo.m_vulkan12Features = vulkan12Features;
				deviceInfo.m_graphicsQueueFamily = static_cast<uint32_t>(graphicsFamilyIndex);
				deviceInfo.m_computeQueueFamily = static_cast<uint32_t>(computeFamilyIndex);
				deviceInfo.m_transferQueueFamily = static_cast<uint32_t>(transferFamilyIndex);
				deviceInfo.m_computeQueuePresentable = static_cast<bool>(computeFamilyPresentable);
				deviceInfo.m_memoryBudgetExtensionSupported = memoryBudgetExtensionSupported;

				suitableDevices.push_back(deviceInfo);
			}
		}

		std::cout << "Found " << suitableDevices.size() << " suitable device(s):" << std::endl;

		size_t deviceIndex = 1;

		if (suitableDevices.empty())
		{
			util::fatalExit("Failed to find a suitable GPU!", EXIT_FAILURE);
		}

		for (size_t i = 0; i < suitableDevices.size(); ++i)
		{
			std::cout << "[" << i << "] " << suitableDevices[i].m_properties.deviceName << std::endl;
		}

		// select first device if there is only a single one
		if (suitableDevices.size() == 1)
		{
			deviceIndex = 0;
		}
		// let the user select a device
		//else
		//{
		//	std::cout << "Select a device:" << std::endl;
		//
		//	do
		//	{
		//		std::cin >> deviceIndex;
		//
		//		if (std::cin.fail() || deviceIndex >= suitableDevices.size())
		//		{
		//			std::cout << "Invalid Index!" << std::endl;
		//			deviceIndex = -1;
		//			std::cin.clear();
		//			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		//		}
		//
		//	} while (deviceIndex >= suitableDevices.size());
		//}

		const auto &selectedDevice = suitableDevices[deviceIndex];

		m_physicalDevice = selectedDevice.m_physicalDevice;

		m_graphicsQueue.m_queue = VK_NULL_HANDLE;
		m_graphicsQueue.m_queueType = QueueType::GRAPHICS;
		m_graphicsQueue.m_timestampValidBits = 0;
		m_graphicsQueue.m_timestampPeriod = selectedDevice.m_properties.limits.timestampPeriod;
		m_graphicsQueue.m_presentable = true;
		m_graphicsQueue.m_queueFamily = selectedDevice.m_graphicsQueueFamily;

		m_computeQueue.m_queue = VK_NULL_HANDLE;
		m_computeQueue.m_queueType = QueueType::COMPUTE;
		m_computeQueue.m_timestampValidBits = 0;
		m_computeQueue.m_timestampPeriod = selectedDevice.m_properties.limits.timestampPeriod;
		m_computeQueue.m_presentable = selectedDevice.m_computeQueuePresentable;
		m_computeQueue.m_queueFamily = selectedDevice.m_computeQueueFamily;

		m_transferQueue.m_queue = VK_NULL_HANDLE;
		m_transferQueue.m_queueType = QueueType::TRANSFER;
		m_transferQueue.m_timestampValidBits = 0;
		m_transferQueue.m_timestampPeriod = selectedDevice.m_properties.limits.timestampPeriod;
		m_transferQueue.m_presentable = false;
		m_transferQueue.m_queueFamily = selectedDevice.m_transferQueueFamily;

		m_properties = selectedDevice.m_properties;
		m_features = selectedDevice.m_features;

		supportsMemoryBudgetExtension = selectedDevice.m_memoryBudgetExtensionSupported;
	}

	// create logical device and retrieve queues
	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies{ m_graphicsQueue.m_queueFamily, m_computeQueue.m_queueFamily, m_transferQueue.m_queueFamily };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.textureCompressionBC = VK_TRUE;
		deviceFeatures.independentBlend = VK_TRUE;
		deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
		deviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
		deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
		deviceFeatures.imageCubeArray = VK_TRUE;
		deviceFeatures.geometryShader = VK_TRUE;

		m_enabledFeatures = deviceFeatures;

		VkPhysicalDeviceVulkan12Features vulkan12Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		vulkan12Features.timelineSemaphore = VK_TRUE;
		vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		vulkan12Features.imagelessFramebuffer = VK_TRUE;
		vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
		//vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
		vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
		vulkan12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
		vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
		vulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
		vulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
		vulkan12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

		constexpr size_t requiredDeviceExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);
		const char *extensions[requiredDeviceExtensionCount + 1];
		for (size_t i = 0; i < requiredDeviceExtensionCount; ++i)
		{
			extensions[i] = deviceExtensions[i];
		}
		extensions[requiredDeviceExtensionCount] = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;

		VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &vulkan12Features };
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = requiredDeviceExtensionCount + (supportsMemoryBudgetExtension ? 1 : 0);
		createInfo.ppEnabledExtensionNames = extensions;

		if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create logical device!", EXIT_FAILURE);
		}

		vkGetDeviceQueue(m_device, m_graphicsQueue.m_queueFamily, 0, &m_graphicsQueue.m_queue);
		vkGetDeviceQueue(m_device, m_computeQueue.m_queueFamily, 0, &m_computeQueue.m_queue);
		vkGetDeviceQueue(m_device, m_transferQueue.m_queueFamily, 0, &m_transferQueue.m_queue);

		uint32_t queueFamilyPropertyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperites(queueFamilyPropertyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperites.data());
		m_graphicsQueue.m_timestampValidBits = queueFamilyProperites[m_graphicsQueue.m_queueFamily].timestampValidBits;
		m_computeQueue.m_timestampValidBits = queueFamilyProperites[m_computeQueue.m_queueFamily].timestampValidBits;
		m_transferQueue.m_timestampValidBits = queueFamilyProperites[m_transferQueue.m_queueFamily].timestampValidBits;
	}

	volkLoadDevice(m_device);

	// try to load VK_EXT_debug_utils functions from device (Radeon GPU Profiler can only see debug info if the functions are loaded from the device)
	//if (g_vulkanDebugCallBackEnabled)
	{
		auto replace = [](PFN_vkVoidFunction oldFp, PFN_vkVoidFunction newFp)
		{
			// loading from device without having RGP running yields nullptr as these functions are instance functions and need to be loaded from the instance instead
			return newFp ? newFp : oldFp;
		};
		vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)replace((PFN_vkVoidFunction)vkCmdBeginDebugUtilsLabelEXT, vkGetDeviceProcAddr(m_device, "vkCmdBeginDebugUtilsLabelEXT"));
		vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)replace((PFN_vkVoidFunction)vkCmdEndDebugUtilsLabelEXT, vkGetDeviceProcAddr(m_device, "vkCmdEndDebugUtilsLabelEXT"));
		vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)replace((PFN_vkVoidFunction)vkCmdInsertDebugUtilsLabelEXT, vkGetDeviceProcAddr(m_device, "vkCmdInsertDebugUtilsLabelEXT"));
		vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)replace((PFN_vkVoidFunction)vkCreateDebugUtilsMessengerEXT, vkGetDeviceProcAddr(m_device, "vkCreateDebugUtilsMessengerEXT"));
		vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)replace((PFN_vkVoidFunction)vkDestroyDebugUtilsMessengerEXT, vkGetDeviceProcAddr(m_device, "vkDestroyDebugUtilsMessengerEXT"));
		vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)replace((PFN_vkVoidFunction)vkQueueBeginDebugUtilsLabelEXT, vkGetDeviceProcAddr(m_device, "vkQueueBeginDebugUtilsLabelEXT"));
		vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)replace((PFN_vkVoidFunction)vkQueueEndDebugUtilsLabelEXT, vkGetDeviceProcAddr(m_device, "vkQueueEndDebugUtilsLabelEXT"));
		vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT)replace((PFN_vkVoidFunction)vkQueueInsertDebugUtilsLabelEXT, vkGetDeviceProcAddr(m_device, "vkQueueInsertDebugUtilsLabelEXT"));
		vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)replace((PFN_vkVoidFunction)vkSetDebugUtilsObjectNameEXT, vkGetDeviceProcAddr(m_device, "vkSetDebugUtilsObjectNameEXT"));
		vkSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)replace((PFN_vkVoidFunction)vkSetDebugUtilsObjectTagEXT, vkGetDeviceProcAddr(m_device, "vkSetDebugUtilsObjectTagEXT"));
		vkSubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)replace((PFN_vkVoidFunction)vkSubmitDebugUtilsMessageEXT, vkGetDeviceProcAddr(m_device, "vkSubmitDebugUtilsMessageEXT"));
	}

	// subgroup properties
	{
		m_subgroupProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };

		VkPhysicalDeviceProperties2 physicalDeviceProperties;
		physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		physicalDeviceProperties.pNext = &m_subgroupProperties;

		vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties);
	}

	m_renderPassCache = new RenderPassCacheVk(m_device);
	m_framebufferCache = new FramebufferCacheVk(m_device);
	m_gpuMemoryAllocator = new MemoryAllocatorVk();
	m_gpuMemoryAllocator->init(m_device, m_physicalDevice, supportsMemoryBudgetExtension);
}

gal::GraphicsDeviceVk::~GraphicsDeviceVk()
{
	vkDeviceWaitIdle(m_device);

	m_gpuMemoryAllocator->destroy();
	delete m_gpuMemoryAllocator;
	delete m_renderPassCache;
	delete m_framebufferCache;
	if (m_swapChain)
	{
		delete m_swapChain;
	}

	vkDestroyDevice(m_device, nullptr);

	//if (g_vulkanDebugCallBackEnabled)
	{
		if (auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"))
		{
			func(m_instance, m_debugUtilsMessenger, nullptr);
		}
	}

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	vkDestroyInstance(m_instance, nullptr);
}

void gal::GraphicsDeviceVk::createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto *memory = m_graphicsPipelineMemoryPool.alloc();
		assert(memory);
		pipelines[i] = new(memory) GraphicsPipelineVk(*this, createInfo[i]);
	}
}

void gal::GraphicsDeviceVk::createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		auto *memory = m_computePipelineMemoryPool.alloc();
		assert(memory);
		pipelines[i] = new(memory) ComputePipelineVk(*this, createInfo[i]);
	}
}

void gal::GraphicsDeviceVk::destroyGraphicsPipeline(GraphicsPipeline *pipeline)
{
	if (pipeline)
	{
		auto *pipelineVk = dynamic_cast<GraphicsPipelineVk *>(pipeline);
		assert(pipelineVk);

		// call destructor and free backing memory
		pipelineVk->~GraphicsPipelineVk();
		m_graphicsPipelineMemoryPool.free(reinterpret_cast<ByteArray<sizeof(GraphicsPipelineVk)> *>(pipelineVk));
	}
}

void gal::GraphicsDeviceVk::destroyComputePipeline(ComputePipeline *pipeline)
{
	if (pipeline)
	{
		auto *pipelineVk = dynamic_cast<ComputePipelineVk *>(pipeline);
		assert(pipelineVk);

		// call destructor and free backing memory
		pipelineVk->~ComputePipelineVk();
		m_computePipelineMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ComputePipelineVk)> *>(pipelineVk));
	}
}

void gal::GraphicsDeviceVk::createCommandListPool(const Queue *queue, CommandListPool **commandListPool)
{
	auto *memory = m_commandListPoolMemoryPool.alloc();
	assert(memory);
	auto *queueVk = dynamic_cast<const QueueVk *>(queue);
	assert(queueVk);
	*commandListPool = new(memory) CommandListPoolVk(*this, *queueVk);
}

void gal::GraphicsDeviceVk::destroyCommandListPool(CommandListPool *commandListPool)
{
	if (commandListPool)
	{
		auto *poolVk = dynamic_cast<CommandListPoolVk *>(commandListPool);
		assert(poolVk);

		// call destructor and free backing memory
		poolVk->~CommandListPoolVk();
		m_commandListPoolMemoryPool.free(reinterpret_cast<ByteArray<sizeof(CommandListPoolVk)> *>(poolVk));
	}
}

void gal::GraphicsDeviceVk::createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool)
{
	auto *memory = m_queryPoolMemoryPool.alloc();
	assert(memory);
	*queryPool = new(memory) QueryPoolVk(m_device, queryType, queryCount, (QueryPipelineStatisticFlags)0);
}

void gal::GraphicsDeviceVk::destroyQueryPool(QueryPool *queryPool)
{
	if (queryPool)
	{
		auto *poolVk = dynamic_cast<QueryPoolVk *>(queryPool);
		assert(poolVk);

		// call destructor and free backing memory
		poolVk->~QueryPoolVk();
		m_queryPoolMemoryPool.free(reinterpret_cast<ByteArray<sizeof(QueryPoolVk)> *>(poolVk));
	}
}

void gal::GraphicsDeviceVk::createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image)
{
	auto *memory = m_imageMemoryPool.alloc();
	assert(memory);

	AllocationCreateInfoVk allocInfo;
	allocInfo.m_requiredFlags = UtilityVk::translateMemoryPropertyFlags(requiredMemoryPropertyFlags);
	allocInfo.m_preferredFlags = UtilityVk::translateMemoryPropertyFlags(preferredMemoryPropertyFlags);
	allocInfo.m_dedicatedAllocation = dedicated;

	VkImageCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	createInfo.flags = UtilityVk::translateImageCreateFlags(imageCreateInfo.m_createFlags);
	createInfo.imageType = UtilityVk::translate(imageCreateInfo.m_imageType);
	createInfo.format = UtilityVk::translate(imageCreateInfo.m_format);
	createInfo.extent = { imageCreateInfo.m_width, imageCreateInfo.m_height, imageCreateInfo.m_depth };
	createInfo.mipLevels = imageCreateInfo.m_levels;
	createInfo.arrayLayers = imageCreateInfo.m_layers;
	createInfo.samples = static_cast<VkSampleCountFlagBits>(imageCreateInfo.m_samples);
	createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	createInfo.usage = UtilityVk::translateImageUsageFlags(imageCreateInfo.m_usageFlags);
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage nativeHandle = VK_NULL_HANDLE;
	AllocationHandleVk allocHandle = 0;

	UtilityVk::checkResult(m_gpuMemoryAllocator->createImage(allocInfo, createInfo, nativeHandle, allocHandle), "Failed to create Image!");

	*image = new(memory) ImageVk(nativeHandle, allocHandle, imageCreateInfo);
}

void gal::GraphicsDeviceVk::createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer)
{
	auto *memory = m_bufferMemoryPool.alloc();
	assert(memory);

	AllocationCreateInfoVk allocInfo;
	allocInfo.m_requiredFlags = UtilityVk::translateMemoryPropertyFlags(requiredMemoryPropertyFlags);
	allocInfo.m_preferredFlags = UtilityVk::translateMemoryPropertyFlags(preferredMemoryPropertyFlags);
	allocInfo.m_dedicatedAllocation = dedicated;

	uint32_t queueFamilyIndices[] =
	{
		m_graphicsQueue.m_queueFamily,
		m_computeQueue.m_queueFamily,
		m_transferQueue.m_queueFamily
	};

	uint32_t queueFamilyIndexCount = 0;
	uint32_t uniqueQueueFamilyIndices[3];

	uniqueQueueFamilyIndices[queueFamilyIndexCount++] = queueFamilyIndices[0];

	if (queueFamilyIndices[1] != queueFamilyIndices[0])
	{
		uniqueQueueFamilyIndices[queueFamilyIndexCount++] = queueFamilyIndices[1];
	}
	if (queueFamilyIndices[2] != queueFamilyIndices[0])
	{
		uniqueQueueFamilyIndices[queueFamilyIndexCount++] = queueFamilyIndices[2];
	}

	VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	createInfo.flags = UtilityVk::translateBufferCreateFlags(bufferCreateInfo.m_createFlags);
	createInfo.size = bufferCreateInfo.m_size;
	createInfo.usage = UtilityVk::translateBufferUsageFlags(bufferCreateInfo.m_usageFlags);
	createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
	createInfo.queueFamilyIndexCount = queueFamilyIndexCount;
	createInfo.pQueueFamilyIndices = uniqueQueueFamilyIndices;

	VkBuffer nativeHandle = VK_NULL_HANDLE;
	AllocationHandleVk allocHandle = 0;

	UtilityVk::checkResult(m_gpuMemoryAllocator->createBuffer(allocInfo, createInfo, nativeHandle, allocHandle), "Failed to create Buffer!");

	*buffer = new(memory) BufferVk(nativeHandle, allocHandle, bufferCreateInfo, m_gpuMemoryAllocator, this);
}

void gal::GraphicsDeviceVk::destroyImage(Image *image)
{
	if (image)
	{
		auto *imageVk = dynamic_cast<ImageVk *>(image);
		assert(imageVk);
		m_gpuMemoryAllocator->destroyImage((VkImage)imageVk->getNativeHandle(), reinterpret_cast<AllocationHandleVk>(imageVk->getAllocationHandle()));

		// call destructor and free backing memory
		imageVk->~ImageVk();
		m_imageMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ImageVk)> *>(imageVk));
	}
}

void gal::GraphicsDeviceVk::destroyBuffer(Buffer *buffer)
{
	if (buffer)
	{
		auto *bufferVk = dynamic_cast<BufferVk *>(buffer);
		assert(bufferVk);
		m_gpuMemoryAllocator->destroyBuffer((VkBuffer)bufferVk->getNativeHandle(), reinterpret_cast<AllocationHandleVk>(bufferVk->getAllocationHandle()));

		// call destructor and free backing memory
		bufferVk->~BufferVk();
		m_bufferMemoryPool.free(reinterpret_cast<ByteArray<sizeof(BufferVk)> *>(bufferVk));
	}
}

void gal::GraphicsDeviceVk::createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView)
{
	auto *memory = m_imageViewMemoryPool.alloc();
	assert(memory);

	*imageView = new(memory) ImageViewVk(m_device, imageViewCreateInfo);
}

void gal::GraphicsDeviceVk::createImageView(Image *image, ImageView **imageView)
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

	// array view
	if (imageViewCreateInfo.m_viewType != ImageViewType::CUBE && imageViewCreateInfo.m_layerCount > 1 || imageViewCreateInfo.m_layerCount > 6)
	{
		ImageViewType arrayType = imageViewCreateInfo.m_viewType;
		switch (imageViewCreateInfo.m_viewType)
		{
		case ImageViewType::_1D:
			arrayType = ImageViewType::_1D_ARRAY;
			break;
		case ImageViewType::_2D:
			arrayType = ImageViewType::_2D_ARRAY;
			break;
		case ImageViewType::_3D:
			// 3d images dont support arrays
			assert(false);
			break;
		case ImageViewType::CUBE:
			arrayType = ImageViewType::CUBE_ARRAY;
			break;
		default:
			assert(false);
			break;
		}

		imageViewCreateInfo.m_viewType = arrayType;
	}

	createImageView(imageViewCreateInfo, imageView);
}

void gal::GraphicsDeviceVk::createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView)
{
	auto *memory = m_bufferViewMemoryPool.alloc();
	assert(memory);

	*bufferView = new(memory) BufferViewVk(m_device, bufferViewCreateInfo);
}

void gal::GraphicsDeviceVk::destroyImageView(ImageView *imageView)
{
	if (imageView)
	{
		auto *viewVk = dynamic_cast<ImageViewVk *>(imageView);
		assert(viewVk);

		// call destructor and free backing memory
		viewVk->~ImageViewVk();
		m_imageViewMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ImageViewVk)> *>(viewVk));
	}
}

void gal::GraphicsDeviceVk::destroyBufferView(BufferView *bufferView)
{
	if (bufferView)
	{
		auto *viewVk = dynamic_cast<BufferViewVk *>(bufferView);
		assert(viewVk);

		// call destructor and free backing memory
		viewVk->~BufferViewVk();
		m_bufferViewMemoryPool.free(reinterpret_cast<ByteArray<sizeof(BufferViewVk)> *>(viewVk));
	}
}

void gal::GraphicsDeviceVk::createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler)
{
	auto *memory = m_samplerMemoryPool.alloc();
	assert(memory);

	*sampler = new(memory) SamplerVk(m_device, samplerCreateInfo);
}

void gal::GraphicsDeviceVk::destroySampler(Sampler *sampler)
{
	if (sampler)
	{
		auto *samplerVk = dynamic_cast<SamplerVk *>(sampler);
		assert(samplerVk);

		// call destructor and free backing memory
		samplerVk->~SamplerVk();
		m_samplerMemoryPool.free(reinterpret_cast<ByteArray<sizeof(SamplerVk)> *>(samplerVk));
	}
}

void gal::GraphicsDeviceVk::createSemaphore(uint64_t initialValue, Semaphore **semaphore)
{
	auto *memory = m_semaphoreMemoryPool.alloc();
	assert(memory);

	*semaphore = new(memory) SemaphoreVk(m_device, initialValue);
}

void gal::GraphicsDeviceVk::destroySemaphore(Semaphore *semaphore)
{
	if (semaphore)
	{
		auto *semaphoreVk = dynamic_cast<SemaphoreVk *>(semaphore);
		assert(semaphoreVk);

		// call destructor and free backing memory
		semaphoreVk->~SemaphoreVk();
		m_semaphoreMemoryPool.free(reinterpret_cast<ByteArray<sizeof(SemaphoreVk)> *>(semaphoreVk));
	}
}

void gal::GraphicsDeviceVk::createDescriptorSetPool(uint32_t maxSets, const DescriptorSetLayout *descriptorSetLayout, DescriptorSetPool **descriptorSetPool)
{
	auto *memory = m_descriptorSetPoolMemoryPool.alloc();
	assert(memory);

	const DescriptorSetLayoutVk *layoutVk = dynamic_cast<const DescriptorSetLayoutVk *>(descriptorSetLayout);
	assert(layoutVk);

	*descriptorSetPool = new(memory) DescriptorSetPoolVk(m_device, maxSets, layoutVk);
}

void gal::GraphicsDeviceVk::destroyDescriptorSetPool(DescriptorSetPool *descriptorSetPool)
{
	if (descriptorSetPool)
	{
		auto *poolVk = dynamic_cast<DescriptorSetPoolVk *>(descriptorSetPool);
		assert(poolVk);

		// call destructor and free backing memory
		poolVk->~DescriptorSetPoolVk();
		m_descriptorSetPoolMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetPoolVk)> *>(poolVk));
	}
}

void gal::GraphicsDeviceVk::createDescriptorSetLayout(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings, DescriptorSetLayout **descriptorSetLayout)
{
	auto *memory = m_descriptorSetLayoutMemoryPool.alloc();
	assert(memory);

	std::vector<VkDescriptorSetLayoutBinding> bindingsVk;
	std::vector<VkDescriptorBindingFlags> bindingFlagsVk;
	bindingsVk.reserve(bindingCount);
	bindingFlagsVk.reserve(bindingCount);

	for (uint32_t i = 0; i < bindingCount; ++i)
	{
		const auto &b = bindings[i];
		VkDescriptorType typeVk = VK_DESCRIPTOR_TYPE_SAMPLER;
		switch (b.m_descriptorType)
		{
		case DescriptorType::SAMPLER:
			typeVk = VK_DESCRIPTOR_TYPE_SAMPLER;
			break;
		case DescriptorType::TEXTURE:
		case DescriptorType::DEPTH_STENCIL_TEXTURE:
			typeVk = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			break;
		case  DescriptorType::RW_TEXTURE:
			typeVk = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			break;
		case  DescriptorType::TYPED_BUFFER:
			typeVk = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			break;
		case  DescriptorType::RW_TYPED_BUFFER:
			typeVk = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
			break;
		case  DescriptorType::CONSTANT_BUFFER:
			typeVk = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			break;
		case  DescriptorType::BYTE_BUFFER:
		case  DescriptorType::RW_BYTE_BUFFER:
		case  DescriptorType::STRUCTURED_BUFFER:
		case  DescriptorType::RW_STRUCTURED_BUFFER:
			typeVk = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			break;
		case  DescriptorType::OFFSET_CONSTANT_BUFFER:
			typeVk = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			break;
		default:
			assert(false);
			break;
		}

		bindingFlagsVk.push_back(UtilityVk::translateDescriptorBindingFlags(b.m_bindingFlags));
		bindingsVk.push_back({ b.m_binding, typeVk , b.m_descriptorCount, UtilityVk::translateShaderStageFlags(b.m_stageFlags), nullptr });
	}

	*descriptorSetLayout = new(memory) DescriptorSetLayoutVk(m_device, bindingCount, bindingsVk.data(), bindingFlagsVk.data());
}

void gal::GraphicsDeviceVk::destroyDescriptorSetLayout(DescriptorSetLayout *descriptorSetLayout)
{
	if (descriptorSetLayout)
	{
		auto *layoutVk = dynamic_cast<DescriptorSetLayoutVk *>(descriptorSetLayout);
		assert(layoutVk);

		// call destructor and free backing memory
		layoutVk->~DescriptorSetLayoutVk();
		m_descriptorSetLayoutMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetLayoutVk)> *>(layoutVk));
	}
}

void gal::GraphicsDeviceVk::createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode, SwapChain **swapChain)
{
	assert(!m_swapChain);
	assert(width && height);
	Queue *queue = nullptr;
	queue = presentQueue == &m_graphicsQueue ? &m_graphicsQueue : queue;
	queue = presentQueue == &m_computeQueue ? &m_computeQueue : queue;
	assert(queue);
	*swapChain = m_swapChain = new SwapChainVk(m_physicalDevice, m_device, m_surface, queue, width, height, fullscreen, presentMode);
}

void gal::GraphicsDeviceVk::destroySwapChain()
{
	assert(m_swapChain);
	delete m_swapChain;
	m_swapChain = nullptr;
}

void gal::GraphicsDeviceVk::waitIdle()
{
	UtilityVk::checkResult(vkDeviceWaitIdle(m_device));
}

void gal::GraphicsDeviceVk::setDebugObjectName(ObjectType objectType, void *object, const char *name)
{
	//if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		info.pObjectName = name;

		switch (objectType)
		{
		case ObjectType::QUEUE:
			info.objectType = VK_OBJECT_TYPE_QUEUE;
			info.objectHandle = (uint64_t)reinterpret_cast<Queue *>(object)->getNativeHandle();
			break;
		case ObjectType::SEMAPHORE:
			info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
			info.objectHandle = (uint64_t)reinterpret_cast<Semaphore *>(object)->getNativeHandle();
			break;
		case ObjectType::COMMAND_LIST:
			info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
			info.objectHandle = (uint64_t)reinterpret_cast<CommandList *>(object)->getNativeHandle();
			break;
		case ObjectType::BUFFER:
			info.objectType = VK_OBJECT_TYPE_BUFFER;
			info.objectHandle = (uint64_t)reinterpret_cast<Buffer *>(object)->getNativeHandle();
			break;
		case ObjectType::IMAGE:
			info.objectType = VK_OBJECT_TYPE_IMAGE;
			info.objectHandle = (uint64_t)reinterpret_cast<Image *>(object)->getNativeHandle();
			break;
		case ObjectType::QUERY_POOL:
			info.objectType = VK_OBJECT_TYPE_QUERY_POOL;
			info.objectHandle = (uint64_t)reinterpret_cast<QueryPool *>(object)->getNativeHandle();
			break;
		case ObjectType::BUFFER_VIEW:
			info.objectType = VK_OBJECT_TYPE_BUFFER_VIEW;
			info.objectHandle = (uint64_t)reinterpret_cast<BufferView *>(object)->getNativeHandle();
			break;
		case ObjectType::IMAGE_VIEW:
			info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
			info.objectHandle = (uint64_t)reinterpret_cast<ImageView *>(object)->getNativeHandle();
			break;
		case ObjectType::GRAPHICS_PIPELINE:
			info.objectType = VK_OBJECT_TYPE_PIPELINE;
			info.objectHandle = (uint64_t)reinterpret_cast<GraphicsPipeline *>(object)->getNativeHandle();
			break;
		case ObjectType::COMPUTE_PIPELINE:
			info.objectType = VK_OBJECT_TYPE_PIPELINE;
			info.objectHandle = (uint64_t)reinterpret_cast<ComputePipeline *>(object)->getNativeHandle();
			break;
		case ObjectType::DESCRIPTOR_SET_LAYOUT:
			info.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
			info.objectHandle = (uint64_t)reinterpret_cast<DescriptorSetLayout *>(object)->getNativeHandle();
			break;
		case ObjectType::SAMPLER:
			info.objectType = VK_OBJECT_TYPE_SAMPLER;
			info.objectHandle = (uint64_t)reinterpret_cast<Sampler *>(object)->getNativeHandle();
			break;
		case ObjectType::DESCRIPTOR_SET_POOL:
			info.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
			info.objectHandle = (uint64_t)reinterpret_cast<DescriptorSetPool *>(object)->getNativeHandle();
			break;
		case ObjectType::DESCRIPTOR_SET:
			info.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
			info.objectHandle = (uint64_t)reinterpret_cast<DescriptorSet *>(object)->getNativeHandle();
			break;
		case ObjectType::COMMAND_LIST_POOL:
			info.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
			info.objectHandle = (uint64_t)reinterpret_cast<CommandListPool *>(object)->getNativeHandle();
			break;
		case ObjectType::SWAPCHAIN:
			info.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
			info.objectHandle = (uint64_t)reinterpret_cast<SwapChain *>(object)->getNativeHandle();
			break;
		default:
			break;
		}

		UtilityVk::checkResult(vkSetDebugUtilsObjectNameEXT(m_device, &info));
	}
}

gal::Queue *gal::GraphicsDeviceVk::getGraphicsQueue()
{
	return &m_graphicsQueue;
}

gal::Queue *gal::GraphicsDeviceVk::getComputeQueue()
{
	return m_computeQueue.m_queue == m_graphicsQueue.m_queue ? &m_graphicsQueue : &m_computeQueue;
}

gal::Queue *gal::GraphicsDeviceVk::getTransferQueue()
{
	return m_transferQueue.m_queue == m_graphicsQueue.m_queue ? &m_graphicsQueue : &m_transferQueue;
}

uint64_t gal::GraphicsDeviceVk::getBufferAlignment(DescriptorType bufferType, uint64_t elementSize) const
{
	switch (bufferType)
	{
	case DescriptorType::TYPED_BUFFER:
		return m_properties.limits.minStorageBufferOffsetAlignment;
	case DescriptorType::RW_TYPED_BUFFER:
		return m_properties.limits.minStorageBufferOffsetAlignment;
	case DescriptorType::CONSTANT_BUFFER:
		return m_properties.limits.minUniformBufferOffsetAlignment;
	case DescriptorType::BYTE_BUFFER:
		return m_properties.limits.minStorageBufferOffsetAlignment;
	case DescriptorType::RW_BYTE_BUFFER:
		return m_properties.limits.minStorageBufferOffsetAlignment;
	case DescriptorType::STRUCTURED_BUFFER:
		return m_properties.limits.minStorageBufferOffsetAlignment;
	case DescriptorType::RW_STRUCTURED_BUFFER:
		return m_properties.limits.minStorageBufferOffsetAlignment;
	case DescriptorType::OFFSET_CONSTANT_BUFFER:
		return m_properties.limits.minUniformBufferOffsetAlignment;
	default:
		assert(false);
		break;
	}
	return uint64_t();
}

uint64_t gal::GraphicsDeviceVk::getMinUniformBufferOffsetAlignment() const
{
	return m_properties.limits.minUniformBufferOffsetAlignment;
}

uint64_t gal::GraphicsDeviceVk::getMinStorageBufferOffsetAlignment() const
{
	return m_properties.limits.minStorageBufferOffsetAlignment;
}

uint64_t gal::GraphicsDeviceVk::getBufferCopyOffsetAlignment() const
{
	return m_properties.limits.optimalBufferCopyOffsetAlignment;
}

uint64_t gal::GraphicsDeviceVk::getBufferCopyRowPitchAlignment() const
{
	return m_properties.limits.optimalBufferCopyRowPitchAlignment;
}

float gal::GraphicsDeviceVk::getMaxSamplerAnisotropy() const
{
	return m_properties.limits.maxSamplerAnisotropy;
}

VkDevice gal::GraphicsDeviceVk::getDevice() const
{
	return m_device;
}

VkRenderPass gal::GraphicsDeviceVk::getRenderPass(const RenderPassDescriptionVk &renderPassDescription)
{
	return m_renderPassCache->getRenderPass(renderPassDescription);
}

VkFramebuffer gal::GraphicsDeviceVk::getFramebuffer(const FramebufferDescriptionVk &framebufferDescription)
{
	return m_framebufferCache->getFramebuffer(framebufferDescription);
}

const VkPhysicalDeviceProperties &gal::GraphicsDeviceVk::getDeviceProperties() const
{
	return m_properties;
}
