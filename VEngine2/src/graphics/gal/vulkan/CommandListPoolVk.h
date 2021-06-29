#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"
#include "Utility/ObjectPool.h"
#include "CommandListVk.h"

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
		DynamicObjectMemoryPool<CommandListVk> m_commandListMemoryPool;
	};
}