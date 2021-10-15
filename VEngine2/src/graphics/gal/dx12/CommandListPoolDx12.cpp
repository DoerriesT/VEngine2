#include "CommandListPoolDx12.h"
#include "QueueDx12.h"
#include "UtilityDx12.h"

gal::CommandListPoolDx12::CommandListPoolDx12(ID3D12Device *device, D3D12_COMMAND_LIST_TYPE cmdListType, const CommandListRecordContextDx12 *recordContext)
	:m_device(device),
	m_cmdListType(cmdListType),
	m_recordContext(recordContext),
	m_liveCmdLists(),
	m_commandListMemoryPool(sizeof(CommandListDx12), 16, "CommandListDx12 Pool Allocator")
{
}

gal::CommandListPoolDx12::~CommandListPoolDx12()
{
	// delete all remaining live command lists
	for (auto &cmdList : m_liveCmdLists)
	{
		ALLOC_DELETE(&m_commandListMemoryPool, cmdList);
	}
	m_liveCmdLists.clear();
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
		CommandListDx12 *cmdListDx = ALLOC_NEW(&m_commandListMemoryPool, CommandListDx12) (m_device, m_cmdListType, m_recordContext);
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

		ALLOC_DELETE(&m_commandListMemoryPool, cmdListDx);

		// remove from m_liveCmdLists
		auto &v = m_liveCmdLists;
		auto it = eastl::find(v.begin(), v.end(), cmdListDx);
		if (it != v.end())
		{
			eastl::swap(v.back(), *it);
			v.erase(v.end() - 1);
		}
	}
}

void gal::CommandListPoolDx12::reset()
{
	for (auto &cmdList : m_liveCmdLists)
	{
		cmdList->reset();
	}
}
