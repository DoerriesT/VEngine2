#include "MemoryAllocatorVk.h"
#include "utility/Utility.h"

gal::MemoryAllocatorVk::MemoryAllocatorVk()
	:m_allocationInfoPool(256)
{
}

void gal::MemoryAllocatorVk::init(VkDevice device, VkPhysicalDevice physicalDevice, bool useMemBudgetExt)
{
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	m_device = device;
	m_physicalDevice = physicalDevice;
	m_bufferImageGranularity = deviceProperties.limits.bufferImageGranularity;
	m_nonCoherentAtomSize = deviceProperties.limits.nonCoherentAtomSize;
	m_useMemoryBudgetExtension = useMemBudgetExt;

	for (size_t i = 0; i < m_memoryProperties.memoryHeapCount; ++i)
	{
		m_heapSizeLimits[i] = m_memoryProperties.memoryHeaps[i].size;
	}

	for (size_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		const uint32_t heapIndex = m_memoryProperties.memoryTypes[i].heapIndex;
		const auto &heap = m_memoryProperties.memoryHeaps[heapIndex];
		m_pools[i].init(m_device, m_physicalDevice, (uint32_t)i, heapIndex, m_bufferImageGranularity, heap.size < MAX_BLOCK_SIZE ? heap.size : MAX_BLOCK_SIZE, &m_heapUsage[heapIndex], m_heapSizeLimits[heapIndex], m_useMemoryBudgetExtension);
	}
}

VkResult gal::MemoryAllocatorVk::alloc(const AllocationCreateInfoVk &allocationCreateInfo, const VkMemoryRequirements &memoryRequirements, const VkMemoryDedicatedAllocateInfo *dedicatedAllocInfo, AllocationHandleVk &allocationHandle)
{
	uint32_t memoryTypeIndex;
	if (findMemoryTypeIndex(memoryRequirements.memoryTypeBits, allocationCreateInfo.m_requiredFlags, allocationCreateInfo.m_preferredFlags, memoryTypeIndex) != VK_SUCCESS)
	{
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	AllocationInfoVk *allocationInfo = m_allocationInfoPool.alloc();

	VkResult result = VK_SUCCESS;

	if (allocationCreateInfo.m_dedicatedAllocation)
	{
		VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, dedicatedAllocInfo };
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

		result = vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &allocationInfo->m_memory);

		if (result == VK_SUCCESS)
		{
			allocationInfo->m_offset = 0;
			allocationInfo->m_size = memoryRequirements.size;
			allocationInfo->m_memoryType = memoryTypeIndex;
			allocationInfo->m_poolIndex = ~size_t(0);
			allocationInfo->m_blockIndex = ~size_t(0);
			allocationInfo->m_mapCount = 0;
			allocationInfo->m_poolData = nullptr;
		}
	}
	else
	{
		result = m_pools[memoryTypeIndex].alloc(memoryRequirements.size, memoryRequirements.alignment, *allocationInfo);
	}

	if (result == VK_SUCCESS)
	{
		allocationHandle = AllocationHandleVk(allocationInfo);
	}
	else
	{
		m_allocationInfoPool.free(allocationInfo);
	}

	return result;
}

VkResult gal::MemoryAllocatorVk::createImage(const AllocationCreateInfoVk &allocationCreateInfo, const VkImageCreateInfo &imageCreateInfo, VkImage &image, AllocationHandleVk &allocationHandle)
{
	VkResult result = vkCreateImage(m_device, &imageCreateInfo, nullptr, &image);

	if (result != VK_SUCCESS)
	{
		return result;
	}

	// get image memory requirements and dedicated memory requirements
	VkMemoryDedicatedRequirements dedicatedRequirements{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
	VkMemoryRequirements2 memoryRequirements2{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicatedRequirements };

	VkImageMemoryRequirementsInfo2 imageRequirementsInfo{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
	imageRequirementsInfo.image = image;

	vkGetImageMemoryRequirements2(m_device, &imageRequirementsInfo, &memoryRequirements2);

	// fill out dedicated alloc info
	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR };
	dedicatedAllocInfo.image = image;

	// allocate memory
	result = alloc(allocationCreateInfo, 
		memoryRequirements2.memoryRequirements, 
		allocationCreateInfo.m_dedicatedAllocation && dedicatedRequirements.prefersDedicatedAllocation ? &dedicatedAllocInfo : nullptr, 
		allocationHandle);

	if (result != VK_SUCCESS)
	{
		vkDestroyImage(m_device, image, nullptr);
		return result;
	}

	AllocationInfoVk *allocationInfo = reinterpret_cast<AllocationInfoVk *>(allocationHandle);

	result = vkBindImageMemory(m_device, image, allocationInfo->m_memory, allocationInfo->m_offset);

	if (result != VK_SUCCESS)
	{
		vkDestroyImage(m_device, image, nullptr);
		free(allocationHandle);
		return result;
	}

	return VK_SUCCESS;
}

VkResult gal::MemoryAllocatorVk::createBuffer(const AllocationCreateInfoVk &allocationCreateInfo, const VkBufferCreateInfo &bufferCreateInfo, VkBuffer &buffer, AllocationHandleVk &allocationHandle)
{
	assert(bufferCreateInfo.size);
	VkResult result = vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &buffer);

	if (result != VK_SUCCESS)
	{
		return result;
	}

	// get buffer memory requirements and dedicated memory requirements
	VkMemoryDedicatedRequirements dedicatedRequirements{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
	VkMemoryRequirements2 memoryRequirements2{ VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicatedRequirements };

	VkBufferMemoryRequirementsInfo2 bufferRequirementsInfo{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 };
	bufferRequirementsInfo.buffer = buffer;

	vkGetBufferMemoryRequirements2(m_device, &bufferRequirementsInfo, &memoryRequirements2);

	// fill out dedicated alloc info
	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR };
	dedicatedAllocInfo.buffer = buffer;

	// allocate memory
	result = alloc(allocationCreateInfo,
		memoryRequirements2.memoryRequirements,
		allocationCreateInfo.m_dedicatedAllocation && dedicatedRequirements.prefersDedicatedAllocation ? &dedicatedAllocInfo : nullptr,
		allocationHandle);

	if (result != VK_SUCCESS)
	{
		vkDestroyBuffer(m_device, buffer, nullptr);
		return result;
	}

	AllocationInfoVk *allocationInfo = reinterpret_cast<AllocationInfoVk *>(allocationHandle);

	result = vkBindBufferMemory(m_device, buffer, allocationInfo->m_memory, allocationInfo->m_offset);

	if (result != VK_SUCCESS)
	{
		vkDestroyBuffer(m_device, buffer, nullptr);
		free(allocationHandle);
		return result;
	}

	return VK_SUCCESS;
}

void gal::MemoryAllocatorVk::free(AllocationHandleVk allocationHandle)
{
	AllocationInfoVk *allocationInfo = reinterpret_cast<AllocationInfoVk *>(allocationHandle);

	// dedicated allocation
	if (allocationInfo->m_poolIndex == ~size_t(0))
	{
		if (allocationInfo->m_mapCount)
		{
			vkUnmapMemory(m_device, allocationInfo->m_memory);
		}
		vkFreeMemory(m_device, allocationInfo->m_memory, nullptr);
	}
	// pool allocation
	else
	{
		m_pools[allocationInfo->m_poolIndex].free(*allocationInfo);
	}

	memset(allocationInfo, 0, sizeof(*allocationInfo));
	m_allocationInfoPool.free(allocationInfo);
}

void gal::MemoryAllocatorVk::destroyImage(VkImage image, AllocationHandleVk allocationHandle)
{
	vkDestroyImage(m_device, image, nullptr);
	free(allocationHandle);
}

void gal::MemoryAllocatorVk::destroyBuffer(VkBuffer buffer, AllocationHandleVk allocationHandle)
{
	vkDestroyBuffer(m_device, buffer, nullptr);
	free(allocationHandle);
}

VkResult gal::MemoryAllocatorVk::mapMemory(AllocationHandleVk allocationHandle, void **data)
{
	AllocationInfoVk *allocationInfo = reinterpret_cast<AllocationInfoVk *>(allocationHandle);

	assert(m_memoryProperties.memoryTypes[allocationInfo->m_memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	VkResult result = VK_SUCCESS;

	// dedicated allocation has a pool index of max uint
	if (allocationInfo->m_poolIndex == ~size_t(0))
	{
		assert(allocationInfo->m_blockIndex == ~size_t(0));

		if (allocationInfo->m_mapCount == 0)
		{
			// dedicated allocations store the mapped pointer in m_poolData
			result = vkMapMemory(m_device, allocationInfo->m_memory, 0, VK_WHOLE_SIZE, 0, &allocationInfo->m_poolData);
		}

		if (result == VK_SUCCESS)
		{
			*data = (char *)allocationInfo->m_poolData;
		}
	}
	// pool allocation
	else
	{
		result = m_pools[allocationInfo->m_poolIndex].mapMemory(allocationInfo->m_blockIndex, allocationInfo->m_offset, data);
	}

	if (result == VK_SUCCESS)
	{
		++allocationInfo->m_mapCount;
	}

	return result;
}

void gal::MemoryAllocatorVk::unmapMemory(AllocationHandleVk allocationHandle)
{
	AllocationInfoVk *allocationInfo = reinterpret_cast<AllocationInfoVk *>(allocationHandle);

	assert(m_memoryProperties.memoryTypes[allocationInfo->m_memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	assert(allocationInfo->m_mapCount);
	--allocationInfo->m_mapCount;

	// dedicated allocation has a pool index of max uint
	if (allocationInfo->m_poolIndex == ~size_t(0))
	{
		assert(allocationInfo->m_blockIndex == ~size_t(0));

		if (allocationInfo->m_mapCount == 0)
		{
			vkUnmapMemory(m_device, allocationInfo->m_memory);
			allocationInfo->m_poolData = nullptr;
		}
	}
	// pool allocation
	else
	{
		m_pools[allocationInfo->m_poolIndex].unmapMemory(allocationInfo->m_blockIndex);
	}
}

gal::AllocationInfoVk gal::MemoryAllocatorVk::getAllocationInfo(AllocationHandleVk allocationHandle)
{
	return *reinterpret_cast<AllocationInfoVk *>(allocationHandle);
}

eastl::vector<gal::MemoryBlockDebugInfoVk> gal::MemoryAllocatorVk::getDebugInfo() const
{
	eastl::vector<MemoryBlockDebugInfoVk> result;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		m_pools[i].getDebugInfo(result);
	}
	return result;
}

size_t gal::MemoryAllocatorVk::getMaximumBlockSize() const
{
	return MAX_BLOCK_SIZE;
}

void gal::MemoryAllocatorVk::getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const
{
	free = 0;
	used = 0;
	wasted = 0;

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		size_t poolFree = 0;
		size_t poolUsed = 0;
		size_t poolWasted = 0;

		m_pools[i].getFreeUsedWastedSizes(poolFree, poolUsed, poolWasted);

		free += poolFree;
		used += poolUsed;
		wasted += poolWasted;
	}
}

void gal::MemoryAllocatorVk::destroy()
{
	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		m_pools[i].destroy();
	}

	m_allocationInfoPool.clear();
}

VkResult gal::MemoryAllocatorVk::findMemoryTypeIndex(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties, VkMemoryPropertyFlags preferredProperties, uint32_t &memoryTypeIndex)
{
	memoryTypeIndex = ~uint32_t(0);
	int bestResultBitCount = -1;

	for (uint32_t memoryIndex = 0; memoryIndex < m_memoryProperties.memoryTypeCount; ++memoryIndex)
	{
		const bool isRequiredMemoryType = memoryTypeBitsRequirement & (1 << memoryIndex);

		const VkMemoryPropertyFlags properties = m_memoryProperties.memoryTypes[memoryIndex].propertyFlags;
		const bool hasRequiredProperties = (properties & requiredProperties) == requiredProperties;

		if (isRequiredMemoryType && hasRequiredProperties)
		{
			uint32_t presentBits = properties & preferredProperties;

			// all preferred bits are present
			if (presentBits == preferredProperties)
			{
				memoryTypeIndex = memoryIndex;
				return VK_SUCCESS;
			}

			// only some bits are present -> count them
			int bitCount = 0;
			for (uint32_t bit = 0; bit < 32; ++bit)
			{
				bitCount += (presentBits & (1 << bit)) >> bit;
			}

			// save memory type with highest bit count of present preferred flags
			if (bitCount > bestResultBitCount)
			{
				bestResultBitCount = bitCount;
				memoryTypeIndex = memoryIndex;
			}
		}
	}

	return memoryTypeIndex != ~uint32_t(0) ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
}

void gal::MemoryAllocatorVk::MemoryPoolVk::init(
	VkDevice device, 
	VkPhysicalDevice physicalDevice, 
	uint32_t memoryType, 
	uint32_t heapIndex, 
	VkDeviceSize bufferImageGranularity, 
	VkDeviceSize preferredBlockSize, 
	VkDeviceSize *heapUsage, 
	VkDeviceSize heapSizeLimit, 
	bool useMemBudgetExt)
{
	m_device = device;
	m_physicalDevice = physicalDevice;
	m_memoryType = memoryType;
	m_heapIndex = heapIndex;
	m_bufferImageGranularity = bufferImageGranularity;
	m_preferredBlockSize = preferredBlockSize;
	m_heapUsage = heapUsage;
	m_heapSizeLimit = heapSizeLimit;
	m_useMemoryBudgetExtension = useMemBudgetExt;

	memset(m_blockSizes, 0, sizeof(m_blockSizes));
	memset(m_memory, 0, sizeof(m_memory));
	memset(m_mappedPtr, 0, sizeof(m_mappedPtr));
	memset(m_mapCount, 0, sizeof(m_mapCount));
	memset(m_allocators, 0, sizeof(m_allocators));
}

VkResult gal::MemoryAllocatorVk::MemoryPoolVk::alloc(VkDeviceSize size, VkDeviceSize alignment, AllocationInfoVk &allocationInfo)
{
	// search existing blocks and try to allocate
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_blockSizes[blockIndex] >= size && allocFromBlock(blockIndex, size, alignment, allocationInfo))
		{
			return VK_SUCCESS;
		}
	}

	// cant allocate from any existing block, create a new one
	{
		// search for index of new block
		size_t blockIndex = ~size_t(0);
		for (size_t i = 0; i < MAX_BLOCKS; ++i)
		{
			if (!m_blockSizes[i])
			{
				blockIndex = i;
				break;
			}
		}

		// maximum number of blocks allocated
		if (blockIndex >= MAX_BLOCKS)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		VkDeviceSize memoryBudget = 0;
		VkDeviceSize usedMemory = 0;
		getBudget(memoryBudget, usedMemory);

		VkDeviceSize maxInBudgetAllocSize = memoryBudget > usedMemory ? (memoryBudget - usedMemory) : 0;

		// try to stay in budet. if there is sufficient memory, try to allocate the preferred size
		VkDeviceSize allocSize = maxInBudgetAllocSize < m_preferredBlockSize ? maxInBudgetAllocSize : m_preferredBlockSize;
		// we still need to allocate enough memory to satisfy the request though
		allocSize = size > allocSize ? size : allocSize;

		VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		memoryAllocateInfo.allocationSize = allocSize;
		memoryAllocateInfo.memoryTypeIndex = m_memoryType;

		if (vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_memory[blockIndex]) != VK_SUCCESS)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		*m_heapUsage += memoryAllocateInfo.allocationSize;

		m_blockSizes[blockIndex] = memoryAllocateInfo.allocationSize;
		char *tlsfAllocatorMemory = m_allocatorMemory + blockIndex * sizeof(TLSFAllocator);
		m_allocators[blockIndex] = new (tlsfAllocatorMemory) TLSFAllocator(static_cast<uint32_t>(memoryAllocateInfo.allocationSize), static_cast<uint32_t>(m_bufferImageGranularity));

		if (allocFromBlock(blockIndex, size, alignment, allocationInfo))
		{
			return VK_SUCCESS;
		}
		else
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
	}
}

void gal::MemoryAllocatorVk::MemoryPoolVk::free(AllocationInfoVk allocationInfo)
{
	m_allocators[allocationInfo.m_blockIndex]->free(allocationInfo.m_poolData);

	assert(m_mapCount[allocationInfo.m_blockIndex] >= allocationInfo.m_mapCount);
	m_mapCount[allocationInfo.m_blockIndex] -= allocationInfo.m_mapCount;

	// TODO: implement proper block freeing heuristic
	//if (m_allocators[allocationInfo.m_blockIndex]->getAllocationCount() == 0)
	//{
	//	vkFreeMemory(m_device, m_memory[allocationInfo.m_blockIndex], nullptr);
	//	m_blockSizes[allocationInfo.m_blockIndex] = 0;
	//	delete m_allocators[allocationInfo.m_blockIndex];
	//	m_allocators[allocationInfo.m_blockIndex] = nullptr;
	//}
}

VkResult gal::MemoryAllocatorVk::MemoryPoolVk::mapMemory(size_t blockIndex, VkDeviceSize offset, void **data)
{
	VkResult result = VK_SUCCESS;
	if (m_mapCount[blockIndex] == 0)
	{
		result = vkMapMemory(m_device, m_memory[blockIndex], 0, VK_WHOLE_SIZE, 0, &m_mappedPtr[blockIndex]);
	}

	if (result == VK_SUCCESS)
	{
		++m_mapCount[blockIndex];
		*data = (char *)m_mappedPtr[blockIndex] + offset;
	}

	return result;
}

void gal::MemoryAllocatorVk::MemoryPoolVk::unmapMemory(size_t blockIndex)
{
	assert(m_mapCount[blockIndex]);
	--m_mapCount[blockIndex];

	if (m_mapCount[blockIndex] == 0)
	{
		vkUnmapMemory(m_device, m_memory[blockIndex]);
		m_mappedPtr[blockIndex] = nullptr;
	}
}

void gal::MemoryAllocatorVk::MemoryPoolVk::getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const
{
	free = 0;
	used = 0;
	wasted = 0;

	for (size_t i = 0; i < MAX_BLOCKS; ++i)
	{
		if (m_blockSizes[i])
		{
			uint32_t blockFree;
			uint32_t blockUsed;
			uint32_t blockWasted;

			m_allocators[i]->getFreeUsedWastedSizes(blockFree, blockUsed, blockWasted);

			free += blockFree;
			used += blockUsed;
			wasted += blockWasted;
		}
	}
}

void gal::MemoryAllocatorVk::MemoryPoolVk::getDebugInfo(eastl::vector<MemoryBlockDebugInfoVk> &result) const
{
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_blockSizes[blockIndex])
		{
			result.push_back({ m_memoryType, m_blockSizes[blockIndex] });
			eastl::vector<TLSFSpanDebugInfo> &spanInfos = result.back().m_spans;

			size_t count;
			m_allocators[blockIndex]->getDebugInfo(&count, nullptr);
			spanInfos.resize(count);
			m_allocators[blockIndex]->getDebugInfo(&count, spanInfos.data());
		}
	}
}

void gal::MemoryAllocatorVk::MemoryPoolVk::destroy()
{
	for (size_t blockIndex = 0; blockIndex < MAX_BLOCKS; ++blockIndex)
	{
		if (m_blockSizes[blockIndex])
		{
			vkFreeMemory(m_device, m_memory[blockIndex], nullptr);

			// we placement newed these, so we need to manually call the dtor
			m_allocators[blockIndex]->~TLSFAllocator();
			m_allocators[blockIndex] = nullptr;
		}
	}

	memset(m_blockSizes, 0, sizeof(m_blockSizes));
	memset(m_memory, 0, sizeof(m_memory));
	memset(m_mappedPtr, 0, sizeof(m_mappedPtr));
	memset(m_mapCount, 0, sizeof(m_mapCount));
	memset(m_allocators, 0, sizeof(m_allocators));
}

bool gal::MemoryAllocatorVk::MemoryPoolVk::allocFromBlock(size_t blockIndex, VkDeviceSize size, VkDeviceSize alignment, AllocationInfoVk &allocationInfo)
{
	// check if there is enough free space
	{
		uint32_t freeSpace;
		uint32_t usedSpace;
		uint32_t wastedSpace;
		m_allocators[blockIndex]->getFreeUsedWastedSizes(freeSpace, usedSpace, wastedSpace);

		if (size > freeSpace)
		{
			return false;
		}
	}

	uint32_t offset;
	if (m_blockSizes[blockIndex] >= size && m_allocators[blockIndex]->alloc(static_cast<uint32_t>(size), static_cast<uint32_t>(alignment), offset, allocationInfo.m_poolData))
	{
		allocationInfo.m_memory = m_memory[blockIndex];
		allocationInfo.m_offset = offset;
		allocationInfo.m_size = size;
		allocationInfo.m_memoryType = m_memoryType;
		allocationInfo.m_poolIndex = m_memoryType;
		allocationInfo.m_blockIndex = blockIndex;
		allocationInfo.m_mapCount = 0;

		return true;
	}
	return false;
}

void gal::MemoryAllocatorVk::MemoryPoolVk::getBudget(VkDeviceSize &budget, VkDeviceSize &usage)
{
	if (m_useMemoryBudgetExtension)
	{
		VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
		VkPhysicalDeviceMemoryProperties2 memProps2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &budgetProperties };
		vkGetPhysicalDeviceMemoryProperties2(m_physicalDevice, &memProps2);

		budget = budgetProperties.heapBudget[m_heapIndex];
		usage = budgetProperties.heapUsage[m_heapIndex];
	}
	else
	{
		budget = static_cast<VkDeviceSize>(m_heapSizeLimit * 0.8f);
		usage = *m_heapUsage;
	}
}
