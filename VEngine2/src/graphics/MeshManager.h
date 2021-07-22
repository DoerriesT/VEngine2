#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include "utility/SpinLock.h"
#include "Mesh.h"
#include "Handles.h"
#include "utility/HandleManager.h"

class ResourceViewRegistry;

class MeshManager
{
public:
	explicit MeshManager(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry) noexcept;
	~MeshManager() noexcept;
	void createSubMeshes(uint32_t count, const SubMeshCreateInfo *subMeshes, SubMeshHandle *handles) noexcept;
	void destroySubMeshes(uint32_t count, const SubMeshHandle *handles, uint64_t frameIndex) noexcept;
	void flushUploadCopies(gal::CommandList *cmdList, uint64_t frameIndex) noexcept;
	void flushDeletionQueue(uint64_t frameIndex) noexcept;
	SubMeshDrawInfo getSubMeshDrawInfo(SubMeshHandle handle) const noexcept;
	SubMeshBufferHandles getSubMeshBufferHandles(SubMeshHandle handle) const noexcept;

private:
	struct Upload
	{
		gal::Buffer *m_stagingBuffer = nullptr;
		gal::Buffer *m_indexBuffer = nullptr;
		gal::Buffer *m_vertexBuffer = nullptr;
		gal::BufferCopy m_indexBufferCopy = {};
		gal::BufferCopy m_vertexBufferCopy = {};
	};

	struct UsedStagingBufer
	{
		gal::Buffer *m_stagingBuffer = nullptr;
		uint64_t m_frameToFreeIn = UINT64_MAX;
	};

	struct FreedMesh
	{
		gal::Buffer *m_indexBuffer = nullptr;
		gal::Buffer *m_vertexBuffer = nullptr;
		uint64_t m_frameToFreeIn = UINT64_MAX;
	};

	mutable SpinLock m_handleManagerMutex;
	mutable SpinLock m_uploadDataMutex;
	mutable SpinLock m_subMeshDrawInfoMutex;
	mutable SpinLock m_subMeshBufferHandlesMutex;
	mutable SpinLock m_meshDeletionQueueMutex;
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	HandleManager m_handleManager;
	eastl::vector<Upload> m_uploads;
	eastl::vector<SubMeshDrawInfo> m_subMeshDrawInfo;
	eastl::vector<SubMeshBufferHandles> m_subMeshBufferHandles;
	eastl::queue<FreedMesh> m_meshDeletionQueue;
	eastl::queue<UsedStagingBufer> m_stagingBufferDeletionQueue;
};