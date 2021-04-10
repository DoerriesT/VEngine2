#include "RendererResources.h"
#include "BufferStackAllocator.h"
#include "ResourceViewRegistry.h"
#include "imgui/imgui.h"

RendererResources::RendererResources(gal::GraphicsDevice *device) noexcept
{
	create();
}

RendererResources::~RendererResources()
{
	destroy();
}

void RendererResources::create() noexcept
{
	// constant buffer
	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::CONSTANT_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableConstantBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableConstantBuffers[1]);

		m_constantBufferStackAllocators[0] = new BufferStackAllocator(m_mappableConstantBuffers[0]);
		m_constantBufferStackAllocators[1] = new BufferStackAllocator(m_mappableConstantBuffers[1]);

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

	// index buffer
	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::INDEX_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableIndexBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableIndexBuffers[1]);

		m_indexBufferStackAllocators[0] = new BufferStackAllocator(m_mappableIndexBuffers[0]);
		m_indexBufferStackAllocators[1] = new BufferStackAllocator(m_mappableIndexBuffers[1]);
	}

	// vertex buffer
	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::VERTEX_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableVertexBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableVertexBuffers[1]);

		m_vertexBufferStackAllocators[0] = new BufferStackAllocator(m_mappableVertexBuffers[0]);
		m_vertexBufferStackAllocators[1] = new BufferStackAllocator(m_mappableVertexBuffers[1]);
	}

	// imgui font texture
	{
		ImGuiIO &io = ImGui::GetIO();

		unsigned char *pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		size_t upload_size = width * height * 4 * sizeof(char);

		assert(upload_size <= m_stagingBuffer->getDescription().m_size);

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

		size_t rowPitch = Utility::alignUp((size_t)(width * 4u), (size_t)m_graphicsDevice->getBufferCopyRowPitchAlignment());

		// Upload to Buffer:
		{
			uint8_t *map = nullptr;
			m_stagingBuffer->map((void **)&map);
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
			m_stagingBuffer->unmap();
		}

		// Copy to Image:
		{
			m_commandListPool->reset();
			m_commandList->begin();
			{
				// transition from UNDEFINED to TRANSFER_DST
				Barrier b0 = Initializers::imageBarrier(m_imGuiFontsTexture, PipelineStageFlagBits::HOST_BIT, PipelineStageFlagBits::TRANSFER_BIT, ResourceState::UNDEFINED, ResourceState::WRITE_IMAGE_TRANSFER);
				m_commandList->barrier(1, &b0);

				BufferImageCopy bufferCopyRegion{};
				bufferCopyRegion.m_imageMipLevel = 0;
				bufferCopyRegion.m_imageBaseLayer = 0;
				bufferCopyRegion.m_imageLayerCount = 1;
				bufferCopyRegion.m_extent.m_width = width;
				bufferCopyRegion.m_extent.m_height = height;
				bufferCopyRegion.m_extent.m_depth = 1;
				bufferCopyRegion.m_bufferOffset = 0;
				bufferCopyRegion.m_bufferRowLength = (uint32_t)rowPitch / 4u; // this is in pixels
				bufferCopyRegion.m_bufferImageHeight = height;

				m_commandList->copyBufferToImage(m_stagingBuffer, m_imGuiFontsTexture, 1, &bufferCopyRegion);

				// transition from TRANSFER_DST to TEXTURE
				Barrier b1 = Initializers::imageBarrier(m_imGuiFontsTexture, PipelineStageFlagBits::TRANSFER_BIT, PipelineStageFlagBits::FRAGMENT_SHADER_BIT, ResourceState::WRITE_IMAGE_TRANSFER, ResourceState::READ_TEXTURE);
				m_commandList->barrier(1, &b1);
			}
			m_commandList->end();
			Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_commandList);
		}

		// Store our identifier
		//io.Fonts->TexID = (ImTextureID)m_imGuiFontsTexture;
	}
}

void RendererResources::destroy() noexcept
{
	delete m_constantBufferStackAllocators[0];
	delete m_constantBufferStackAllocators[1];
	delete m_indexBufferStackAllocators[0];
	delete m_indexBufferStackAllocators[1];
	delete m_vertexBufferStackAllocators[0];
	delete m_vertexBufferStackAllocators[1];

	m_device->destroyBuffer(m_mappableConstantBuffers[0]);
	m_device->destroyBuffer(m_mappableConstantBuffers[1]);
	m_device->destroyBuffer(m_mappableIndexBuffers[0]);
	m_device->destroyBuffer(m_mappableIndexBuffers[1]);
	m_device->destroyBuffer(m_mappableVertexBuffers[0]);
	m_device->destroyBuffer(m_mappableVertexBuffers[1]);

	m_device->destroyDescriptorSetPool(m_offsetBufferDescriptorSetPool);
	m_device->destroyDescriptorSetLayout(m_offsetBufferDescriptorSetLayout);
}
