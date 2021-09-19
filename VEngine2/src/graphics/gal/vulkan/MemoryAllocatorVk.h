#pragma once
#include "volk.h"
#include <EASTL/vector.h>
#include "utility/ObjectPool.h"
#include "utility/TLSFAllocator.h"

namespace gal
{
	typedef struct AllocationHandleVk_t *AllocationHandleVk;

	struct AllocationCreateInfoVk
	{
		VkMemoryPropertyFlags m_requiredFlags;
		VkMemoryPropertyFlags m_preferredFlags;
		bool m_dedicatedAllocation;
	};

	struct AllocationInfoVk
	{
		VkDeviceMemory m_memory;
		VkDeviceSize m_offset;
		VkDeviceSize m_size;
		uint32_t m_memoryType;
		size_t m_poolIndex;
		size_t m_blockIndex;
		size_t m_mapCount;
		void *m_poolData;
	};

	struct MemoryBlockDebugInfoVk
	{
		uint32_t m_memoryType;
		size_t m_allocationSize;
		eastl::vector<TLSFSpanDebugInfo> m_spans;
	};

	class MemoryAllocatorVk
	{
	public:
		MemoryAllocatorVk();
		void init(VkDevice device, VkPhysicalDevice physicalDevice, bool useMemBudgetExt);
		VkResult alloc(const AllocationCreateInfoVk &allocationCreateInfo, const VkMemoryRequirements &memoryRequirements, const VkMemoryDedicatedAllocateInfo *dedicatedAllocInfo, AllocationHandleVk &allocationHandle);
		VkResult createImage(const AllocationCreateInfoVk &allocationCreateInfo, const VkImageCreateInfo &imageCreateInfo, VkImage &image, AllocationHandleVk &allocationHandle);
		VkResult createBuffer(const AllocationCreateInfoVk &allocationCreateInfo, const VkBufferCreateInfo &bufferCreateInfo, VkBuffer &buffer, AllocationHandleVk &allocationHandle);
		void free(AllocationHandleVk allocationHandle);
		void destroyImage(VkImage image, AllocationHandleVk allocationHandle);
		void destroyBuffer(VkBuffer buffer, AllocationHandleVk allocationHandle);
		VkResult mapMemory(AllocationHandleVk allocationHandle, void **data);
		void unmapMemory(AllocationHandleVk allocationHandle);
		AllocationInfoVk getAllocationInfo(AllocationHandleVk allocationHandle);
		eastl::vector<MemoryBlockDebugInfoVk> getDebugInfo() const;
		size_t getMaximumBlockSize() const;
		void getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const;
		void destroy();

	private:
		enum
		{
			MAX_BLOCK_SIZE = 256 * 1024 * 1024
		};

		class MemoryPoolVk
		{
		public:
			void init(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t memoryType, uint32_t heapIndex, VkDeviceSize bufferImageGranularity, VkDeviceSize preferredBlockSize, VkDeviceSize *heapUsage, VkDeviceSize heapSizeLimit, bool useMemBudgetExt);
			VkResult alloc(VkDeviceSize size, VkDeviceSize alignment, AllocationInfoVk &allocationInfo);
			void free(AllocationInfoVk allocationInfo);
			VkResult mapMemory(size_t blockIndex, VkDeviceSize offset, void **data);
			void unmapMemory(size_t blockIndex);
			void getFreeUsedWastedSizes(size_t &free, size_t &used, size_t &wasted) const;
			void getDebugInfo(eastl::vector<MemoryBlockDebugInfoVk> &result) const;
			void destroy();

		private:
			enum
			{
				MAX_BLOCKS = 16
			};
			VkDevice m_device = VK_NULL_HANDLE;
			VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
			uint32_t m_memoryType = -1;
			uint32_t m_heapIndex = -1;
			VkDeviceSize m_bufferImageGranularity = 0;
			VkDeviceSize m_preferredBlockSize = 0;
			VkDeviceSize *m_heapUsage = nullptr;
			VkDeviceSize m_heapSizeLimit = 0;
			VkDeviceSize m_blockSizes[MAX_BLOCKS] = {};
			VkDeviceMemory m_memory[MAX_BLOCKS] = {};
			void *m_mappedPtr[MAX_BLOCKS] = {};
			size_t m_mapCount[MAX_BLOCKS] = {};
			TLSFAllocator *m_allocators[MAX_BLOCKS] = {};
			alignas(TLSFAllocator) char m_allocatorMemory[MAX_BLOCKS * sizeof(TLSFAllocator)];
			bool m_useMemoryBudgetExtension = false;

			bool allocFromBlock(size_t blockIndex, VkDeviceSize size, VkDeviceSize alignment, AllocationInfoVk &allocationInfo);
			void getBudget(VkDeviceSize &budget, VkDeviceSize &usage);
		};

		VkDevice m_device = VK_NULL_HANDLE;
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceMemoryProperties m_memoryProperties = {};
		VkDeviceSize m_bufferImageGranularity = 0;
		VkDeviceSize m_nonCoherentAtomSize = 0;
		VkDeviceSize m_heapSizeLimits[VK_MAX_MEMORY_HEAPS] = {};
		VkDeviceSize m_heapUsage[VK_MAX_MEMORY_HEAPS] = {};
		MemoryPoolVk m_pools[VK_MAX_MEMORY_TYPES] = {};
		DynamicObjectPool<AllocationInfoVk> m_allocationInfoPool;
		bool m_useMemoryBudgetExtension = false;

		VkResult findMemoryTypeIndex(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties, VkMemoryPropertyFlags preferredProperties, uint32_t &memoryTypeIndex);
	};
}