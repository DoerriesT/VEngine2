#include "RendererResources.h"
#include "BufferStackAllocator.h"
#include "ResourceViewRegistry.h"
#include "imgui/imgui.h"
#include "utility/Utility.h"
#include "gal/Initializers.h"
#include <assert.h>
#include "Material.h"
#include "ProxyMeshes.h"
#include "TextureLoader.h"
#include "filesystem/VirtualFileSystem.h"

RendererResources::RendererResources(gal::GraphicsDevice *device, ResourceViewRegistry *resourceViewRegistry, TextureLoader *textureLoader) noexcept
	:m_device(device),
	m_resourceViewRegistry(resourceViewRegistry)
{
	m_device->createCommandListPool(m_device->getGraphicsQueue(), &m_commandListPool);
	m_commandListPool->allocate(1, &m_commandList);

	// constant buffer
	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::CONSTANT_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableConstantBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableConstantBuffers[1]);

		m_constantBufferStackAllocators[0] = new BufferStackAllocator(m_device, m_mappableConstantBuffers[0]);
		m_constantBufferStackAllocators[1] = new BufferStackAllocator(m_device, m_mappableConstantBuffers[1]);

		gal::DescriptorSetLayoutBinding binding{ gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, gal::ShaderStageFlags::ALL_STAGES };
		m_device->createDescriptorSetLayout(1, &binding, &m_offsetBufferDescriptorSetLayout);
		m_device->createDescriptorSetPool(2, m_offsetBufferDescriptorSetLayout, &m_offsetBufferDescriptorSetPool);
		m_offsetBufferDescriptorSetPool->allocateDescriptorSets(2, m_offsetBufferDescriptorSets);

		for (size_t i = 0; i < 2; ++i)
		{
			gal::DescriptorSetUpdate update{ 0, 0, 1, gal::DescriptorType::OFFSET_CONSTANT_BUFFER };
			update.m_bufferInfo1 = { m_mappableConstantBuffers[i], 0, 1024/*m_mappableConstantBuffers[i]->getDescription().m_size*/, 0 };
			m_offsetBufferDescriptorSets[i]->update(1, &update);
		}
	}

	// shader resource buffer
	{
		gal::BufferCreateInfo createInfo{ 8 * 1024 * 1024, {}, gal::BufferUsageFlags::TYPED_BUFFER_BIT | gal::BufferUsageFlags::BYTE_BUFFER_BIT | gal::BufferUsageFlags::STRUCTURED_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableShaderResourceBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableShaderResourceBuffers[1]);

		m_shaderResourceBufferStackAllocators[0] = new BufferStackAllocator(m_device, m_mappableShaderResourceBuffers[0]);
		m_shaderResourceBufferStackAllocators[1] = new BufferStackAllocator(m_device, m_mappableShaderResourceBuffers[1]);
	}

	// index buffer
	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::INDEX_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableIndexBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableIndexBuffers[1]);

		m_indexBufferStackAllocators[0] = new BufferStackAllocator(m_device, m_mappableIndexBuffers[0]);
		m_indexBufferStackAllocators[1] = new BufferStackAllocator(m_device, m_mappableIndexBuffers[1]);
	}

	// vertex buffer
	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::VERTEX_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableVertexBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableVertexBuffers[1]);

		m_vertexBufferStackAllocators[0] = new BufferStackAllocator(m_device, m_mappableVertexBuffers[0]);
		m_vertexBufferStackAllocators[1] = new BufferStackAllocator(m_device, m_mappableVertexBuffers[1]);
	}

	// imgui font texture
	{
		ImGuiIO &io = ImGui::GetIO();

		unsigned char *pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		size_t upload_size = width * height * 4 * sizeof(char);

		gal::Buffer *stagingBuffer = nullptr;
		{
			gal::BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = upload_size;
			bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_SRC_BIT;

			m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::HOST_COHERENT_BIT | gal::MemoryPropertyFlags::HOST_VISIBLE_BIT, {}, false, &stagingBuffer);
		}

		// create image and view
		{
			// create image
			gal::ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_width = width;
			imageCreateInfo.m_height = height;
			imageCreateInfo.m_depth = 1;
			imageCreateInfo.m_levels = 1;
			imageCreateInfo.m_layers = 1;
			imageCreateInfo.m_samples = gal::SampleCount::_1;
			imageCreateInfo.m_imageType = gal::ImageType::_2D;
			imageCreateInfo.m_format = gal::Format::R8G8B8A8_UNORM;
			imageCreateInfo.m_createFlags = {};
			imageCreateInfo.m_usageFlags = gal::ImageUsageFlags::TRANSFER_DST_BIT | gal::ImageUsageFlags::TEXTURE_BIT;
			imageCreateInfo.m_optimizedClearValue.m_color = {};

			m_device->createImage(imageCreateInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_imguiFontTexture);

			// create view
			m_device->createImageView(m_imguiFontTexture, &m_imguiFontTextureView);

			m_imguiFontTextureViewHandle = m_resourceViewRegistry->createTextureViewHandle(m_imguiFontTextureView);
		}

		size_t rowPitch = util::alignUp((size_t)(width * 4u), (size_t)m_device->getBufferCopyRowPitchAlignment());

		// Upload to Buffer:
		{
			uint8_t *map = nullptr;
			stagingBuffer->map((void **)&map);
			{
				// keep track of current offset in staging buffer
				size_t currentOffset = 0;

				const uint8_t *srcData = pixels;

				for (size_t row = 0; row < height; ++row)
				{
					memcpy(map + currentOffset, srcData, width * 4u);
					srcData += width * 4u;
					currentOffset += rowPitch;
				}
			}
			stagingBuffer->unmap();
		}

		// Copy to Image:
		{
			m_commandListPool->reset();
			m_commandList->begin();
			{
				// transition from UNDEFINED to TRANSFER_DST
				gal::Barrier b0 = gal::Initializers::imageBarrier(
					m_imguiFontTexture,
					gal::PipelineStageFlags::HOST_BIT,
					gal::PipelineStageFlags::TRANSFER_BIT,
					gal::ResourceState::UNDEFINED,
					gal::ResourceState::WRITE_TRANSFER);
				m_commandList->barrier(1, &b0);

				gal::BufferImageCopy bufferCopyRegion{};
				bufferCopyRegion.m_imageMipLevel = 0;
				bufferCopyRegion.m_imageBaseLayer = 0;
				bufferCopyRegion.m_imageLayerCount = 1;
				bufferCopyRegion.m_extent.m_width = width;
				bufferCopyRegion.m_extent.m_height = height;
				bufferCopyRegion.m_extent.m_depth = 1;
				bufferCopyRegion.m_bufferOffset = 0;
				bufferCopyRegion.m_bufferRowLength = (uint32_t)rowPitch / 4u; // this is in pixels
				bufferCopyRegion.m_bufferImageHeight = height;

				m_commandList->copyBufferToImage(stagingBuffer, m_imguiFontTexture, 1, &bufferCopyRegion);

				// transition from TRANSFER_DST to TEXTURE
				gal::Barrier b1 = gal::Initializers::imageBarrier(
					m_imguiFontTexture,
					gal::PipelineStageFlags::TRANSFER_BIT,
					gal::PipelineStageFlags::PIXEL_SHADER_BIT,
					gal::ResourceState::WRITE_TRANSFER,
					gal::ResourceState::READ_RESOURCE);
				m_commandList->barrier(1, &b1);
			}
			m_commandList->end();
			gal::Initializers::submitSingleTimeCommands(m_device->getGraphicsQueue(), m_commandList);
		}

		// free staging buffer
		m_device->destroyBuffer(stagingBuffer);

		// Store our identifier
		ImGui::GetIO().Fonts->SetTexID((ImTextureID)(size_t)m_imguiFontTextureViewHandle);
	}

	// blue noise texture
	{
		const char *k_filepath = "/assets/textures/blue_noise.dds";
		eastl::vector<char> fileData(VirtualFileSystem::get().size(k_filepath));
		VirtualFileSystem::get().readFile(k_filepath, fileData.size(), fileData.data(), true);
		textureLoader->load(fileData.size(), fileData.data(), k_filepath, &m_blueNoiseTexture, &m_blueNoiseTextureView);
		m_blueNoiseTextureViewHandle = m_resourceViewRegistry->createTextureViewHandle(m_blueNoiseTextureView);
	}

	// proxy mesh vertex/index buffer
	{
		gal::BufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.m_size = IcoSphereProxyMesh::vertexDataSize
			+ Cone180ProxyMesh::vertexDataSize
			+ Cone135ProxyMesh::vertexDataSize
			+ Cone90ProxyMesh::vertexDataSize
			+ Cone45ProxyMesh::vertexDataSize
			+ BoxProxyMesh::vertexDataSize;
		vertexBufferInfo.m_createFlags = {};
		vertexBufferInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_DST_BIT | gal::BufferUsageFlags::VERTEX_BUFFER_BIT;

		m_device->createBuffer(vertexBufferInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_proxyMeshVertexBuffer);

		gal::BufferCreateInfo indexBufferInfo{};
		indexBufferInfo.m_size = IcoSphereProxyMesh::indexDataSize
			+ Cone180ProxyMesh::indexDataSize
			+ Cone135ProxyMesh::indexDataSize
			+ Cone90ProxyMesh::indexDataSize
			+ Cone45ProxyMesh::indexDataSize
			+ BoxProxyMesh::indexDataSize;
		indexBufferInfo.m_createFlags = {};
		indexBufferInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_DST_BIT | gal::BufferUsageFlags::INDEX_BUFFER_BIT;

		m_device->createBuffer(indexBufferInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_proxyMeshIndexBuffer);

		gal::Buffer *stagingBuffer = nullptr;
		{
			gal::BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = vertexBufferInfo.m_size + indexBufferInfo.m_size;
			bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_SRC_BIT;

			m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::HOST_COHERENT_BIT | gal::MemoryPropertyFlags::HOST_VISIBLE_BIT, {}, false, &stagingBuffer);
		}

		uint8_t *data;
		size_t srcOffset = 0;

		stagingBuffer->map((void **)&data);

		// ico sphere vertex buffer
		memcpy(data + srcOffset, IcoSphereProxyMesh::vertexData, IcoSphereProxyMesh::vertexDataSize);
		srcOffset += IcoSphereProxyMesh::vertexDataSize;

		// cone 180 vertex buffer
		memcpy(data + srcOffset, Cone180ProxyMesh::vertexData, Cone180ProxyMesh::vertexDataSize);
		srcOffset += Cone180ProxyMesh::vertexDataSize;

		// cone 135 vertex buffer
		memcpy(data + srcOffset, Cone135ProxyMesh::vertexData, Cone135ProxyMesh::vertexDataSize);
		srcOffset += Cone135ProxyMesh::vertexDataSize;

		// cone 90 vertex buffer
		memcpy(data + srcOffset, Cone90ProxyMesh::vertexData, Cone90ProxyMesh::vertexDataSize);
		srcOffset += Cone90ProxyMesh::vertexDataSize;

		// cone 45 vertex buffer
		memcpy(data + srcOffset, Cone45ProxyMesh::vertexData, Cone45ProxyMesh::vertexDataSize);
		srcOffset += Cone45ProxyMesh::vertexDataSize;

		// box vertex buffer
		memcpy(data + srcOffset, BoxProxyMesh::vertexData, BoxProxyMesh::vertexDataSize);
		srcOffset += BoxProxyMesh::vertexDataSize;

		// ico sphere index buffer
		memcpy(data + srcOffset, IcoSphereProxyMesh::indexData, IcoSphereProxyMesh::indexDataSize);
		srcOffset += IcoSphereProxyMesh::indexDataSize;

		// cone 180 index buffer
		memcpy(data + srcOffset, Cone180ProxyMesh::indexData, Cone180ProxyMesh::indexDataSize);
		srcOffset += Cone180ProxyMesh::indexDataSize;

		// cone 135 index buffer
		memcpy(data + srcOffset, Cone135ProxyMesh::indexData, Cone135ProxyMesh::indexDataSize);
		srcOffset += Cone135ProxyMesh::indexDataSize;

		// cone 90 index buffer
		memcpy(data + srcOffset, Cone90ProxyMesh::indexData, Cone90ProxyMesh::indexDataSize);
		srcOffset += Cone90ProxyMesh::indexDataSize;

		// cone 45 index buffer
		memcpy(data + srcOffset, Cone45ProxyMesh::indexData, Cone45ProxyMesh::indexDataSize);
		srcOffset += Cone45ProxyMesh::indexDataSize;

		// box index buffer
		memcpy(data + srcOffset, BoxProxyMesh::indexData, BoxProxyMesh::indexDataSize);
		srcOffset += BoxProxyMesh::indexDataSize;

		stagingBuffer->unmap();


		m_commandListPool->reset();
		m_commandList->begin();
		{
			{
				gal::Barrier barriers[]
				{
					gal::Initializers::bufferBarrier(stagingBuffer,
					gal::PipelineStageFlags::TOP_OF_PIPE_BIT, gal::PipelineStageFlags::TRANSFER_BIT,
					gal::ResourceState::UNDEFINED, gal::ResourceState::READ_TRANSFER),

					gal::Initializers::bufferBarrier(m_proxyMeshVertexBuffer,
					gal::PipelineStageFlags::TOP_OF_PIPE_BIT, gal::PipelineStageFlags::TRANSFER_BIT,
					gal::ResourceState::UNDEFINED, gal::ResourceState::WRITE_TRANSFER),

					gal::Initializers::bufferBarrier(m_proxyMeshIndexBuffer,
					gal::PipelineStageFlags::TOP_OF_PIPE_BIT, gal::PipelineStageFlags::TRANSFER_BIT,
					gal::ResourceState::UNDEFINED, gal::ResourceState::WRITE_TRANSFER),
				};

				m_commandList->barrier(3, barriers);
			}

			gal::BufferCopy vertexCopy = { 0, 0, vertexBufferInfo.m_size };
			m_commandList->copyBuffer(stagingBuffer, m_proxyMeshVertexBuffer, 1, &vertexCopy);

			gal::BufferCopy indexCopy = { vertexBufferInfo.m_size, 0, indexBufferInfo.m_size };
			m_commandList->copyBuffer(stagingBuffer, m_proxyMeshIndexBuffer, 1, &indexCopy);

			{
				gal::Barrier barriers[]
				{
					gal::Initializers::bufferBarrier(m_proxyMeshVertexBuffer,
					gal::PipelineStageFlags::TRANSFER_BIT, gal::PipelineStageFlags::VERTEX_INPUT_BIT,
					gal::ResourceState::WRITE_TRANSFER, gal::ResourceState::READ_VERTEX_BUFFER),

					gal::Initializers::bufferBarrier(m_proxyMeshIndexBuffer,
					gal::PipelineStageFlags::TRANSFER_BIT, gal::PipelineStageFlags::VERTEX_INPUT_BIT,
					gal::ResourceState::WRITE_TRANSFER, gal::ResourceState::READ_INDEX_BUFFER),
				};

				m_commandList->barrier(2, barriers);
			}
		}
		m_commandList->end();
		gal::Initializers::submitSingleTimeCommands(m_device->getGraphicsQueue(), m_commandList);

		// free staging buffer
		m_device->destroyBuffer(stagingBuffer);

		m_icoSphereProxyMeshInfo.m_firstIndex = 0;
		m_icoSphereProxyMeshInfo.m_indexCount = static_cast<uint32_t>(IcoSphereProxyMesh::indexCount);
		m_icoSphereProxyMeshInfo.m_vertexOffset = 0;

		m_cone180ProxyMeshInfo.m_firstIndex = m_icoSphereProxyMeshInfo.m_firstIndex + m_icoSphereProxyMeshInfo.m_indexCount;
		m_cone180ProxyMeshInfo.m_indexCount = static_cast<uint32_t>(Cone180ProxyMesh::indexCount);
		m_cone180ProxyMeshInfo.m_vertexOffset = m_icoSphereProxyMeshInfo.m_vertexOffset + static_cast<uint32_t>(IcoSphereProxyMesh::vertexDataSize / (sizeof(float) * 3));

		m_cone135ProxyMeshInfo.m_firstIndex = m_cone180ProxyMeshInfo.m_firstIndex + m_cone180ProxyMeshInfo.m_indexCount;
		m_cone135ProxyMeshInfo.m_indexCount = static_cast<uint32_t>(Cone135ProxyMesh::indexCount);
		m_cone135ProxyMeshInfo.m_vertexOffset = m_cone180ProxyMeshInfo.m_vertexOffset + static_cast<uint32_t>(Cone180ProxyMesh::vertexDataSize / (sizeof(float) * 3));

		m_cone90ProxyMeshInfo.m_firstIndex = m_cone135ProxyMeshInfo.m_firstIndex + m_cone135ProxyMeshInfo.m_indexCount;
		m_cone90ProxyMeshInfo.m_indexCount = static_cast<uint32_t>(Cone90ProxyMesh::indexCount);
		m_cone90ProxyMeshInfo.m_vertexOffset = m_cone135ProxyMeshInfo.m_vertexOffset + static_cast<uint32_t>(Cone135ProxyMesh::vertexDataSize / (sizeof(float) * 3));

		m_cone45ProxyMeshInfo.m_firstIndex = m_cone90ProxyMeshInfo.m_firstIndex + m_cone90ProxyMeshInfo.m_indexCount;
		m_cone45ProxyMeshInfo.m_indexCount = static_cast<uint32_t>(Cone45ProxyMesh::indexCount);
		m_cone45ProxyMeshInfo.m_vertexOffset = m_cone90ProxyMeshInfo.m_vertexOffset + static_cast<uint32_t>(Cone90ProxyMesh::vertexDataSize / (sizeof(float) * 3));

		m_boxProxyMeshInfo.m_firstIndex = m_cone45ProxyMeshInfo.m_firstIndex + m_cone45ProxyMeshInfo.m_indexCount;
		m_boxProxyMeshInfo.m_indexCount = static_cast<uint32_t>(BoxProxyMesh::indexCount);
		m_boxProxyMeshInfo.m_vertexOffset = m_cone45ProxyMeshInfo.m_vertexOffset + static_cast<uint32_t>(Cone45ProxyMesh::vertexDataSize / (sizeof(float) * 3));
	}

	// materials buffer
	{
		gal::BufferCreateInfo createInfo{ sizeof(MaterialGPU) * 1024 * 32, {}, gal::BufferUsageFlags::STRUCTURED_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_materialsBuffer);
		gal::DescriptorBufferInfo bufferInfo{};
		bufferInfo.m_buffer = m_materialsBuffer;
		bufferInfo.m_range = createInfo.m_size;
		bufferInfo.m_structureByteStride = sizeof(MaterialGPU);
		m_materialsBufferViewHandle = m_resourceViewRegistry->createStructuredBufferViewHandle(bufferInfo);
	}
}

RendererResources::~RendererResources()
{
	delete m_constantBufferStackAllocators[0];
	delete m_constantBufferStackAllocators[1];
	delete m_shaderResourceBufferStackAllocators[0];
	delete m_shaderResourceBufferStackAllocators[1];
	delete m_indexBufferStackAllocators[0];
	delete m_indexBufferStackAllocators[1];
	delete m_vertexBufferStackAllocators[0];
	delete m_vertexBufferStackAllocators[1];

	m_device->destroyBuffer(m_mappableConstantBuffers[0]);
	m_device->destroyBuffer(m_mappableConstantBuffers[1]);
	m_device->destroyBuffer(m_mappableShaderResourceBuffers[0]);
	m_device->destroyBuffer(m_mappableShaderResourceBuffers[1]);
	m_device->destroyBuffer(m_mappableIndexBuffers[0]);
	m_device->destroyBuffer(m_mappableIndexBuffers[1]);
	m_device->destroyBuffer(m_mappableVertexBuffers[0]);
	m_device->destroyBuffer(m_mappableVertexBuffers[1]);

	m_resourceViewRegistry->destroyHandle(m_imguiFontTextureViewHandle);
	m_device->destroyImageView(m_imguiFontTextureView);
	m_device->destroyImage(m_imguiFontTexture);

	m_resourceViewRegistry->destroyHandle(m_blueNoiseTextureViewHandle);
	m_device->destroyImageView(m_blueNoiseTextureView);
	m_device->destroyImage(m_blueNoiseTexture);

	m_device->destroyBuffer(m_proxyMeshVertexBuffer);
	m_device->destroyBuffer(m_proxyMeshIndexBuffer);

	m_resourceViewRegistry->destroyHandle(m_materialsBufferViewHandle);
	m_device->destroyBuffer(m_materialsBuffer);

	m_device->destroyDescriptorSetPool(m_offsetBufferDescriptorSetPool);
	m_device->destroyDescriptorSetLayout(m_offsetBufferDescriptorSetLayout);

	m_commandListPool->free(1, &m_commandList);
	m_device->destroyCommandListPool(m_commandListPool);
}