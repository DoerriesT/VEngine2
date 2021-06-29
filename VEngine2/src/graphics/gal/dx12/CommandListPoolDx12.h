#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>
#include "Utility/ObjectPool.h"
#include "CommandListDx12.h"

namespace gal
{
	class QueueDx12;

	class CommandListPoolDx12 : public CommandListPool
	{
	public:
		explicit CommandListPoolDx12(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE cmdListType, const CommandListRecordContextDx12 *recordContext);
		CommandListPoolDx12(CommandListPoolDx12 &) = delete;
		CommandListPoolDx12(CommandListPoolDx12 &&) = delete;
		CommandListPoolDx12 &operator= (const CommandListPoolDx12 &) = delete;
		CommandListPoolDx12 &operator= (const CommandListPoolDx12 &&) = delete;
		~CommandListPoolDx12();
		void *getNativeHandle() const override;
		void allocate(uint32_t count, CommandList **commandLists) override;
		void free(uint32_t count, CommandList **commandLists) override;
		void reset() override;
	private:
		ID3D12Device *m_device;
		D3D12_COMMAND_LIST_TYPE m_cmdListType;
		const CommandListRecordContextDx12 *m_recordContext;
		std::vector<CommandListDx12 *> m_liveCmdLists;
		DynamicObjectMemoryPool<CommandListDx12> m_commandListMemoryPool;
	};
}