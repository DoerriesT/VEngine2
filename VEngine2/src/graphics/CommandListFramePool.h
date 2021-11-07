#pragma once
#include "gal/FwdDecl.h"
#include <stdint.h>
#include <EASTL/vector.h>
#include "utility/DeletedCopyMove.h"

class CommandListFramePool
{
public:
	explicit CommandListFramePool() noexcept = default;
	DELETED_COPY_MOVE(CommandListFramePool);
	~CommandListFramePool() noexcept;
	void init(gal::GraphicsDevice *device, uint32_t allocationChunkSize = 64) noexcept;
	gal::CommandList *acquire(gal::Queue *queue) noexcept;
	void reset() noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	uint32_t m_allocationChunkSize = 64;
	gal::CommandListPool *m_commandListPools[3] = {};
	eastl::vector<gal::CommandList *> m_commandLists[3] = {};
	uint32_t m_nextFreeCmdList[3] = {};
};