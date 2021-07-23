#include "MeshManager.h"
#include "utility/Utility.h"
#include "ResourceViewRegistry.h"
#include "gal/Initializers.h"
#include "Log.h"

MeshManager::MeshManager(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry)
{
	m_subMeshDrawInfo.resize(16);
	m_subMeshBufferHandles.resize(16);
}

MeshManager::~MeshManager() noexcept
{
	for (auto &handles : m_subMeshBufferHandles)
	{
		m_viewRegistry->destroyHandle(handles.m_positionsBufferViewHandle);
		m_viewRegistry->destroyHandle(handles.m_normalsBufferViewHandle);
		m_viewRegistry->destroyHandle(handles.m_tangentsBufferViewHandle);
		m_viewRegistry->destroyHandle(handles.m_texCoordsBufferViewHandle);
		m_device->destroyBuffer(handles.m_indexBuffer);
		m_device->destroyBuffer(handles.m_vertexBuffer);
	}

	// empty the complete deletion queue
	while (!m_meshDeletionQueue.empty())
	{
		auto &freedMesh = m_meshDeletionQueue.front();

		m_device->destroyBuffer(freedMesh.m_indexBuffer);
		m_device->destroyBuffer(freedMesh.m_vertexBuffer);

		m_meshDeletionQueue.pop();
	}
}

void MeshManager::createSubMeshes(uint32_t count, const SubMeshCreateInfo *subMeshes, SubMeshHandle *handles) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		const SubMeshCreateInfo &subMesh = subMeshes[i];

		// allocate handle
		{
			LOCK_HOLDER(m_handleManagerMutex);
			handles[i] = (SubMeshHandle)m_handleManager.allocate();
		}

		if (!handles[i])
		{
			Log::err("MeshManager: Failed to allocate SubMeshHandle!");
			continue;
		}

		SubMeshDrawInfo subMeshDrawInfo{};
		subMeshDrawInfo.m_indexCount = subMesh.m_indexCount;
		subMeshDrawInfo.m_firstIndex = 0;
		subMeshDrawInfo.m_vertexOffset = 0;
		subMeshDrawInfo.m_vertexCount = subMesh.m_vertexCount;

		SubMeshBufferHandles subMeshBufferHandles{};

		const size_t indexBufferSize = subMesh.m_indexCount * sizeof(subMesh.m_indices[0]);
		const size_t positionsBufferSize = subMesh.m_vertexCount * sizeof(subMesh.m_positions[0]) * 3;
		const size_t normalsBufferSize = subMesh.m_vertexCount * sizeof(subMesh.m_normals[0]) * 3;
		const size_t tangentsBufferSize = subMesh.m_vertexCount * sizeof(subMesh.m_tangents[0]) * 4;
		const size_t texCoordsBufferSize = subMesh.m_vertexCount * sizeof(subMesh.m_texCoords[0]) * 2;

		const size_t alignedIndexBufferSize = util::alignUp<size_t>(indexBufferSize, sizeof(float) * 4);
		const size_t alignedPositionsBufferSize = util::alignUp<size_t>(positionsBufferSize, sizeof(float) * 4);
		const size_t alignedNormalsBufferSize = util::alignUp<size_t>(normalsBufferSize, sizeof(float) * 4);
		const size_t alignedTangentsBufferSize = util::alignUp<size_t>(tangentsBufferSize, sizeof(float) * 4);
		const size_t alignedTexCoordsBufferSize = util::alignUp<size_t>(texCoordsBufferSize, sizeof(float) * 4);
		const size_t alignedVertexBufferSize = alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize + alignedTexCoordsBufferSize;

		// create index buffer
		{
			gal::BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = indexBufferSize;
			bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::INDEX_BUFFER_BIT | gal::BufferUsageFlags::TRANSFER_DST_BIT;

			m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &subMeshBufferHandles.m_indexBuffer);
			m_device->setDebugObjectName(gal::ObjectType::BUFFER, subMeshBufferHandles.m_indexBuffer, "Index Buffer");
		}

		// create vertex buffer
		gal::Buffer *vertexBuffer = nullptr;
		{
			gal::BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = alignedVertexBufferSize;
			bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::VERTEX_BUFFER_BIT | gal::BufferUsageFlags::TRANSFER_DST_BIT;

			m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &subMeshBufferHandles.m_vertexBuffer);
			m_device->setDebugObjectName(gal::ObjectType::BUFFER, subMeshBufferHandles.m_vertexBuffer, "Vertex Buffer");
		}

		// create staging buffer
		gal::Buffer *stagingBuffer = nullptr;
		{
			gal::BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = alignedIndexBufferSize + alignedVertexBufferSize;
			bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_SRC_BIT;

			m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::HOST_COHERENT_BIT | gal::MemoryPropertyFlags::HOST_VISIBLE_BIT, {}, false, &stagingBuffer);
			m_device->setDebugObjectName(gal::ObjectType::BUFFER, stagingBuffer, "Mesh Staging Buffer");
		}

		Upload upload = {};
		upload.m_stagingBuffer = stagingBuffer;
		upload.m_indexBuffer = subMeshBufferHandles.m_indexBuffer;
		upload.m_vertexBuffer = subMeshBufferHandles.m_vertexBuffer;
		upload.m_indexBufferCopy.m_srcOffset = 0;
		upload.m_indexBufferCopy.m_dstOffset = 0;
		upload.m_indexBufferCopy.m_size = indexBufferSize;
		upload.m_vertexBufferCopy.m_srcOffset = alignedIndexBufferSize;
		upload.m_vertexBufferCopy.m_dstOffset = 0;
		upload.m_vertexBufferCopy.m_size = alignedVertexBufferSize;

		// copy data to staging buffer
		{
			uint8_t *data;
			stagingBuffer->map((void **)&data);
			{
				// keep track of current offset in staging buffer
				size_t currentOffset = 0;

				// indices
				memcpy(data + currentOffset, subMesh.m_indices, indexBufferSize);
				currentOffset += alignedIndexBufferSize;

				// positions
				memcpy(data + currentOffset, subMesh.m_positions, positionsBufferSize);
				currentOffset += alignedPositionsBufferSize;

				// normals
				memcpy(data + currentOffset, subMesh.m_normals, normalsBufferSize);
				currentOffset += alignedNormalsBufferSize;

				// tangents
				memcpy(data + currentOffset, subMesh.m_tangents, tangentsBufferSize);
				currentOffset += alignedTangentsBufferSize;

				// tex coords
				memcpy(data + currentOffset, subMesh.m_texCoords, texCoordsBufferSize);
				currentOffset += alignedTexCoordsBufferSize;

			}
			stagingBuffer->unmap();
		}

		{
			LOCK_HOLDER(m_uploadDataMutex);
			m_uploads.push_back(eastl::move(upload));
		}

		//// create views
		//{
		//	gal::DescriptorBufferInfo descriptorBufferInfo{};
		//	descriptorBufferInfo.m_buffer = subMeshBufferHandles.m_vertexBuffer;
		//
		//	// positions
		//	descriptorBufferInfo.m_offset = 0;
		//	descriptorBufferInfo.m_range = positionsBufferSize;
		//	descriptorBufferInfo.m_structureByteStride = sizeof(float) * 3;
		//	subMeshBufferHandles.m_positionsBufferViewHandle = m_viewRegistry->createStructuredBufferViewHandle(descriptorBufferInfo);
		//
		//	// normals
		//	descriptorBufferInfo.m_offset = alignedPositionsBufferSize;
		//	descriptorBufferInfo.m_range = normalsBufferSize;
		//	descriptorBufferInfo.m_structureByteStride = sizeof(float) * 3;
		//	subMeshBufferHandles.m_normalsBufferViewHandle = m_viewRegistry->createStructuredBufferViewHandle(descriptorBufferInfo);
		//
		//	// tangents
		//	descriptorBufferInfo.m_offset = alignedPositionsBufferSize + alignedNormalsBufferSize;
		//	descriptorBufferInfo.m_range = tangentsBufferSize;
		//	descriptorBufferInfo.m_structureByteStride = sizeof(float) * 4;
		//	subMeshBufferHandles.m_tangentsBufferViewHandle = m_viewRegistry->createStructuredBufferViewHandle(descriptorBufferInfo);
		//
		//	// tex coords
		//	descriptorBufferInfo.m_offset = alignedPositionsBufferSize + alignedNormalsBufferSize + alignedTangentsBufferSize;
		//	descriptorBufferInfo.m_range = texCoordsBufferSize;
		//	descriptorBufferInfo.m_structureByteStride = sizeof(float) * 2;
		//	subMeshBufferHandles.m_texCoordsBufferViewHandle = m_viewRegistry->createStructuredBufferViewHandle(descriptorBufferInfo);
		//}

		// store draw info
		{
			LOCK_HOLDER(m_subMeshDrawInfoMutex);
			if (handles[i] > m_subMeshDrawInfo.size())
			{
				m_subMeshDrawInfo.resize((size_t)(m_subMeshDrawInfo.size() * 1.5));
			}

			m_subMeshDrawInfo[handles[i] - 1] = subMeshDrawInfo;
		}

		// store buffer handles
		{
			LOCK_HOLDER(m_subMeshBufferHandlesMutex);
			if (handles[i] > m_subMeshBufferHandles.size())
			{
				m_subMeshBufferHandles.resize((size_t)(m_subMeshBufferHandles.size() * 1.5));
			}

			m_subMeshBufferHandles[handles[i] - 1] = subMeshBufferHandles;
		}
	}
}

void MeshManager::destroySubMeshes(uint32_t count, const SubMeshHandle *handles, uint64_t frameIndex) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		FreedMesh freedMesh{};
		{
			LOCK_HOLDER(m_subMeshBufferHandlesMutex);
			const bool validHandle = handles[i] != 0 && handles[i] <= m_subMeshBufferHandles.size();
			if (!validHandle)
			{
				continue;
			}

			auto &meshBufferHandles = m_subMeshBufferHandles[handles[i] - 1];

			m_viewRegistry->destroyHandle(meshBufferHandles.m_positionsBufferViewHandle);
			m_viewRegistry->destroyHandle(meshBufferHandles.m_normalsBufferViewHandle);
			m_viewRegistry->destroyHandle(meshBufferHandles.m_tangentsBufferViewHandle);
			m_viewRegistry->destroyHandle(meshBufferHandles.m_texCoordsBufferViewHandle);

			freedMesh.m_indexBuffer = meshBufferHandles.m_indexBuffer;
			freedMesh.m_vertexBuffer = meshBufferHandles.m_vertexBuffer;
			freedMesh.m_frameToFreeIn = frameIndex + 2; // delete mesh in 2 frames from now

			meshBufferHandles = {};
		}
		
		{
			LOCK_HOLDER(m_meshDeletionQueueMutex);
			m_meshDeletionQueue.push(freedMesh);
		}

		{
			LOCK_HOLDER(m_handleManagerMutex);
			m_handleManager.free(handles[i]);
		}
	}
}

void MeshManager::flushUploadCopies(gal::CommandList *cmdList, uint64_t frameIndex) noexcept
{
	// delete old staging buffers
	while (!m_stagingBufferDeletionQueue.empty())
	{
		auto &usedBuffer = m_stagingBufferDeletionQueue.front();

		if (usedBuffer.m_frameToFreeIn <= frameIndex)
		{
			m_device->destroyBuffer(usedBuffer.m_stagingBuffer);
			m_stagingBufferDeletionQueue.pop();
		}
		else
		{
			// since frameIndex is monotonically increasing, we can just break out of the loop once we encounter
			// the first item where we have not reached the frame to delete it in yet.
			break;
		}
	}

	LOCK_HOLDER(m_uploadDataMutex);

	if (m_uploads.empty())
	{
		return;
	}

	eastl::vector<gal::Barrier> barriers;
	barriers.reserve(m_uploads.size() * 2);

	// transition from UNDEFINED to TRANSFER_DST
	{
		for (auto &upload : m_uploads)
		{
			barriers.push_back(gal::Initializers::bufferBarrier(upload.m_indexBuffer,
				gal::PipelineStageFlags::TOP_OF_PIPE_BIT,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::ResourceState::UNDEFINED,
				gal::ResourceState::WRITE_TRANSFER));

			barriers.push_back(gal::Initializers::bufferBarrier(upload.m_vertexBuffer,
				gal::PipelineStageFlags::TOP_OF_PIPE_BIT,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::ResourceState::UNDEFINED,
				gal::ResourceState::WRITE_TRANSFER));
		}

		cmdList->barrier((uint32_t)barriers.size(), barriers.data());
	}

	// upload data
	for (auto &upload : m_uploads)
	{
		cmdList->copyBuffer(upload.m_stagingBuffer, upload.m_indexBuffer, 1, &upload.m_indexBufferCopy);
		cmdList->copyBuffer(upload.m_stagingBuffer, upload.m_vertexBuffer, 1, &upload.m_vertexBufferCopy);

		// delete staging buffer in 2 frames from now
		m_stagingBufferDeletionQueue.push({ upload.m_stagingBuffer, (frameIndex + 2) });
	}

	// transition form TRANSFER_DST to READ_INDEX_BUFFER/READ_RESOURCE
	{
		barriers.clear();
		for (auto &upload : m_uploads)
		{
			barriers.push_back(gal::Initializers::bufferBarrier(upload.m_indexBuffer,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::PipelineStageFlags::VERTEX_INPUT_BIT,
				gal::ResourceState::WRITE_TRANSFER,
				gal::ResourceState::READ_INDEX_BUFFER));

			barriers.push_back(gal::Initializers::bufferBarrier(upload.m_vertexBuffer,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::PipelineStageFlags::VERTEX_SHADER_BIT | gal::PipelineStageFlags::PIXEL_SHADER_BIT | gal::PipelineStageFlags::COMPUTE_SHADER_BIT | gal::PipelineStageFlags::VERTEX_INPUT_BIT,
				gal::ResourceState::WRITE_TRANSFER,
				gal::ResourceState::READ_VERTEX_BUFFER));
		}

		cmdList->barrier((uint32_t)barriers.size(), barriers.data());
	}

	m_uploads.clear();
}

void MeshManager::flushDeletionQueue(uint64_t frameIndex) noexcept
{
	LOCK_HOLDER(m_meshDeletionQueueMutex);

	while (!m_meshDeletionQueue.empty())
	{
		auto &freedMesh = m_meshDeletionQueue.front();

		if (freedMesh.m_frameToFreeIn <= frameIndex)
		{
			m_device->destroyBuffer(freedMesh.m_indexBuffer);
			m_device->destroyBuffer(freedMesh.m_vertexBuffer);
			m_meshDeletionQueue.pop();
		}
		else
		{
			// since frameIndex is monotonically increasing, we can just break out of the loop once we encounter
			// the first item where we have not reached the frame to delete it in yet.
			break;
		}
	}
}

SubMeshDrawInfo MeshManager::getSubMeshDrawInfo(SubMeshHandle handle) const noexcept
{
	LOCK_HOLDER(m_subMeshDrawInfoMutex);
	const bool validHandle = handle != 0 && handle <= m_subMeshDrawInfo.size();
	if (!validHandle)
	{
		return SubMeshDrawInfo();
	}
	return m_subMeshDrawInfo[handle - 1];
}

SubMeshBufferHandles MeshManager::getSubMeshBufferHandles(SubMeshHandle handle) const noexcept
{
	LOCK_HOLDER(m_subMeshBufferHandlesMutex);
	const bool validHandle = handle != 0 && handle <= m_subMeshBufferHandles.size();
	if (!validHandle)
	{
		return SubMeshBufferHandles();
	}
	return m_subMeshBufferHandles[handle - 1];
}