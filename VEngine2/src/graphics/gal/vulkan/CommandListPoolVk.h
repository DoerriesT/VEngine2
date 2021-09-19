#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"
#include "utility/allocator/PoolAllocator.h"
#include "CommandListVk.h"
#include <EASTL/fixed_vector.h>

namespace gal
{
	class GraphicsDeviceVk;
	class QueueVk;

	class CommandListPoolVk : public CommandListPool
	{
	public:
		explicit CommandListPoolVk(GraphicsDeviceVk &device, const QueueVk &queue);
		CommandListPoolVk(CommandListPoolVk &) = delete;
		CommandListPoolVk(CommandListPoolVk &&) = delete;
		CommandListPoolVk &operator= (const CommandListPoolVk &) = delete;
		CommandListPoolVk &operator= (const CommandListPoolVk &&) = delete;
		~CommandListPoolVk();
		void *getNativeHandle() const override;
		void allocate(uint32_t count, CommandList **commandLists) override;
		void free(uint32_t count, CommandList **commandLists) override;
		void reset() override;
	private:
		GraphicsDeviceVk *m_device;
		VkCommandPool m_commandPool;
		DynamicPoolAllocator m_commandListMemoryPool;
		eastl::fixed_vector<CommandListVk *, 32> m_liveCommandLists;
	};
}