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

		m_resultImageTextureViewHandle = m_viewRegistry->createTextureViewHandle(m_resultImageView);

		m_resultImageResourceState = gal::ResourceState::UNDEFINED;
		m_resultImagePipelineStages = gal::PipelineStageFlags::TOP_OF_PIPE_BIT;
	}
	
	// depth buffer
	{
		gal::ImageCreateInfo createInfo{};
		createInfo.m_width = width;
		createInfo.m_height = height;
		createInfo.m_format = gal::Format::D32_SFLOAT;
		createInfo.m_usageFlags = gal::ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT | gal::ImageUsageFlags::TEXTURE_BIT;

		m_device->createImage(createInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, true, &m_depthBufferImage);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE, m_depthBufferImage, "Render View Depth Buffer");

		m_device->createImageView(m_depthBufferImage, &m_depthBufferImageView);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE_VIEW, m_depthBufferImageView, "Render View Depth Buffer View");

		m_depthBufferTextureViewHandle = m_viewRegistry->createTextureViewHandle(m_depthBufferImageView);

		m_depthBufferImageResourceState = gal::ResourceState::UNDEFINED;
		m_depthBufferImagePipelineStages = gal::PipelineStageFlags::TOP_OF_PIPE_BIT;
	}

	// skinning matrices buffer
	for (size_t i = 0; i < 2; ++i)
	{
		size_t bufferSize = sizeof(float) * 16 * 1024;
		
		gal::BufferCreateInfo createInfo{};
		createInfo.m_size = bufferSize;
		createInfo.m_usageFlags = gal::BufferUsageFlags::STRUCTURED_BUFFER_BIT;

		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, {}, false, &m_skinningMatricesBuffers[i]);
		m_device->setDebugObjectName(gal::ObjectType::BUFFER, m_skinningMatricesBuffers[i], i == 0 ? "Skinning Matrices Buffer 0" : "Skinning Matrices Buffer 1");

		gal::DescriptorBufferInfo descriptorBufferInfo{ m_skinningMatricesBuffers[i], 0, bufferSize, sizeof(float) * 16 };
		m_skinningMatricesBufferViewHandles[i] = m_viewRegistry->createStructuredBufferViewHandle(descriptorBufferInfo);
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

	// depth buffer
	{
		m_viewRegistry->destroyHandle(m_depthBufferTextureViewHandle);
		m_depthBufferTextureViewHandle = {};
		m_device->destroyImageView(m_depthBufferImageView);
		m_device->destroyImage(m_depthBufferImage);
	}

	// skinning matrices buffer
	for (size_t i = 0; i < 2; ++i)
	{
		m_viewRegistry->destroyHandle(m_skinningMatricesBufferViewHandles[i]);
		m_skinningMatricesBufferViewHandles[i] = {};
		m_device->destroyBuffer(m_skinningMatricesBuffers[i]);
	}
}
