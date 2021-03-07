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
		createInfo.m_usageFlags = gal::ImageUsageFlags::COLOR_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT | gal::ImageUsageFlags::TRANSFER_SRC_BIT;

		m_device->createImage(createInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, true, &m_resultImage);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE, m_resultImage, "Render View Result Image");

		m_device->createImageView(m_resultImage, &m_resultImageView);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE_VIEW, m_resultImageView, "Render View Result Image View");

		m_viewRegistry->createTextureViewHandle(m_resultImageView);

		m_resultImageResourceState = gal::ResourceState::UNDEFINED;
		m_resultImagePipelineStages = gal::PipelineStageFlags::TOP_OF_PIPE_BIT;
	}
	
}

void RenderViewResources::destroy() noexcept
{
	// result image
	{
		m_viewRegistry->destroyHandle(m_resultImageTextureViewHandle);
		m_device->destroyImageView(m_resultImageView);
		m_device->destroyImage(m_resultImage);
	}
}
