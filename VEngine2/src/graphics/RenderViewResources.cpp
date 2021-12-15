#include "RenderViewResources.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "ResourceViewRegistry.h"

RenderViewResources::RenderViewResources(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, uint32_t width, uint32_t height) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry),
	m_width(width),
	m_height(height)
{
	create(width, height);
}

RenderViewResources::~RenderViewResources()
{
	destroy();
}

void RenderViewResources::resize(uint32_t width, uint32_t height) noexcept
{
	destroy();
	m_width = width;
	m_height = height;
	create(width, height);
}

void RenderViewResources::create(uint32_t width, uint32_t height) noexcept
{
	// result image
	{
		gal::ImageCreateInfo createInfo{};
		createInfo.m_width = width;
		createInfo.m_height = height;
		createInfo.m_format = gal::Format::B8G8R8A8_UNORM;
		createInfo.m_usageFlags = gal::ImageUsageFlags::COLOR_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT | gal::ImageUsageFlags::RW_TEXTURE_BIT | gal::ImageUsageFlags::TRANSFER_SRC_BIT;

		m_device->createImage(createInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, true, &m_resultImage);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE, m_resultImage, "Render View Result Image");

		m_device->createImageView(m_resultImage, &m_resultImageView);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE_VIEW, m_resultImageView, "Render View Result Image View");

		m_resultImageTextureViewHandle = m_viewRegistry->createTextureViewHandle(m_resultImageView);

		m_resultImageState[0] = {};
	}
	
	// exposure data buffer
	{
		gal::BufferCreateInfo createInfo{};
		createInfo.m_size = sizeof(float) * 4;
		createInfo.m_usageFlags = gal::BufferUsageFlags::BYTE_BUFFER_BIT | gal::BufferUsageFlags::RW_BYTE_BUFFER_BIT | gal::BufferUsageFlags::TRANSFER_DST_BIT;

		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_exposureDataBuffer);
		m_device->setDebugObjectName(gal::ObjectType::BUFFER, m_exposureDataBuffer, "Exposure Buffer");
	}

	// picking readback buffers
	for (size_t i = 0; i < 2; ++i)
	{
		gal::BufferCreateInfo createInfo{};
		createInfo.m_size = sizeof(uint32_t) * 4;
		createInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_DST_BIT;

		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_CACHED_BIT, {}, false, &m_pickingDataReadbackBuffers[i]);
		m_device->setDebugObjectName(gal::ObjectType::BUFFER, m_pickingDataReadbackBuffers[i], i == 0 ? "Picking Data Readback Buffer 0" : "Picking Data Readback Buffer 1");
	}

	// TAA images
	for (size_t i = 0; i < 2; ++i)
	{
		gal::ImageCreateInfo createInfo{};
		createInfo.m_width = width;
		createInfo.m_height = height;
		createInfo.m_format = gal::Format::R16G16B16A16_SFLOAT;
		createInfo.m_usageFlags = gal::ImageUsageFlags::COLOR_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT | gal::ImageUsageFlags::RW_TEXTURE_BIT | gal::ImageUsageFlags::TRANSFER_SRC_BIT;

		m_device->createImage(createInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, &m_temporalAAImages[i]);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE, m_temporalAAImages[i], i == 0 ? "Temporal AA Image 0" : "Temporal AA Image 1");

		m_temporalAAImageStates[i] = {};
	}
}

void RenderViewResources::destroy() noexcept
{
	// result image
	{
		m_viewRegistry->destroyHandle(m_resultImageTextureViewHandle);
		m_resultImageTextureViewHandle = {};
		m_device->destroyImageView(m_resultImageView);
		m_device->destroyImage(m_resultImage);
	}

	m_device->destroyBuffer(m_exposureDataBuffer);

	for (size_t i = 0; i < 2; ++i)
	{
		m_device->destroyBuffer(m_pickingDataReadbackBuffers[i]);
	}

	for (size_t i = 0; i < 2; ++i)
	{
		m_device->destroyImage(m_temporalAAImages[i]);
	}
}
