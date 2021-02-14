#include "CommandListPoolDx12.h"
#include "QueueDx12.h"
#include "UtilityDx12.h"
#include "utility/ContainerUtility.h"

gal::CommandListPoolDx12::CommandListPoolDx12(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE cmdListType, const CommandListRecordContextDx12 *recordContext)
	:m_device(device),
	m_cmdListType(cmdListType),
	m_recordContext(recordContext),
	m_liveCmdLists(),
	m_commandListMemoryPool(16)
{
	m_liveCmdLists.reserve(16);
}

gal::CommandListPoolDx12::~CommandListPoolDx12()
{
	for (auto &cmdList : m_liveCmdLists)
	{
		// call destructor
		cmdList->~CommandListDx12();
	}
}

void *gal::CommandListPoolDx12::getNativeHandle() const
{
	// this class doesnt map to any native D3D12 concept
	return nullptr;
}

void gal::CommandListPoolDx12::allocate(uint32_t count, CommandList **commandLists)
{
	for (size_t i = 0; i < count; ++i)
	{
		auto *memory = m_commandListMemoryPool.alloc();
		assert(memory);

		CommandListDx12 *cmdListDx = new (memory) CommandListDx12(m_device, m_cmdListType, m_recordContext);
		commandLists[i] = cmdListDx;

		// add to vector of live command lists, so we can call the destructor of every live command list
		// when this CommandListPoolDx12 instance is destroyed
		m_liveCmdLists.push_back(cmdListDx);
	}
}

void gal::CommandListPoolDx12::free(uint32_t count, CommandList **commandLists)
{
	for (size_t i = 0; i < count; ++i)
	{
		auto *cmdListDx = dynamic_cast<CommandListDx12 *>(commandLists[i]);
		assert(cmdListDx);

		// call destructor and free backing memory
		cmdListDx->~CommandListDx12();
		m_commandListMemoryPool.free(reinterpret_cast<ByteArray<sizeof(CommandListDx12)> *>(cmdListDx));

		ContainerUtility::quickRemove(m_liveCmdLists, cmdListDx);
	}
}

void gal::CommandListPoolDx12::reset()
{
	for (auto &cmdList : m_liveCmdLists)
	{
		cmdList->reset();
	}
}
