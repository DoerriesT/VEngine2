#include "TextureLoader.h"
#include <gli/load.hpp>
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include "Log.h"
#include "utility/Utility.h"

TextureLoader::TextureLoader(gal::GraphicsDevice *device) noexcept
	:m_device(device)
{
}

TextureLoader::~TextureLoader() noexcept
{
	// destroy all staging buffers, both in pending uploads and in the deletion queue.
	// this assumes that GraphicsDevice::waitIdle() has been called before.

	for (auto &upload : m_uploads)
	{
		m_device->destroyBuffer(upload.m_stagingBuffer);
	}

	while (!m_deletionQueue.empty())
	{
		auto &buffer = m_deletionQueue.front();
		m_device->destroyBuffer(buffer.m_stagingBuffer);
		m_deletionQueue.pop();
	}
}

bool TextureLoader::load(size_t fileSize, const char *fileData, const char *textureName, gal::Image **image, gal::ImageView **imageView) noexcept
{
	gli::texture gliTex(gli::load(fileData, fileSize));

	if (gliTex.empty())
	{
		Log::warn("Failed to load texture: \"%s\"", textureName);
		return false;
	}

	// create image
	{
		gal::ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = static_cast<uint32_t>(gliTex.extent().x);
		imageCreateInfo.m_height = static_cast<uint32_t>(gliTex.extent().y);
		imageCreateInfo.m_depth = static_cast<uint32_t>(gliTex.extent().z);
		imageCreateInfo.m_layers = static_cast<uint32_t>(gliTex.layers());
		imageCreateInfo.m_levels = static_cast<uint32_t>(gliTex.levels());
		imageCreateInfo.m_samples = gal::SampleCount::_1;
		imageCreateInfo.m_imageType = (gliTex.target() == gli::TARGET_2D || gliTex.target() == gli::TARGET_2D_ARRAY) ? gal::ImageType::_2D : gal::ImageType::_3D;
		imageCreateInfo.m_format = static_cast<gal::Format>(gliTex.format());
		imageCreateInfo.m_createFlags = {};
		imageCreateInfo.m_usageFlags = gal::ImageUsageFlags::TEXTURE_BIT | gal::ImageUsageFlags::TRANSFER_DST_BIT;
		imageCreateInfo.m_optimizedClearValue = {};

		m_device->createImage(imageCreateInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, image);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE, *image, textureName);

		// create image view
		m_device->createImageView(*image, imageView);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE_VIEW, *imageView, textureName);
	}

	// create staging buffer
	gal::Buffer *stagingBuffer = nullptr;
	{
		gal::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = gliTex.size() * 2; // account for padding. TODO: calculate actual size requirement
		bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_SRC_BIT;

		m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::HOST_COHERENT_BIT | gal::MemoryPropertyFlags::HOST_VISIBLE_BIT, {}, false, &stagingBuffer);
	}

	Upload upload = {};
	upload.m_imageSubresourceRange.m_baseMipLevel = 0;
	upload.m_imageSubresourceRange.m_levelCount = static_cast<uint32_t>(gliTex.levels());
	upload.m_imageSubresourceRange.m_baseArrayLayer = 0;
	upload.m_imageSubresourceRange.m_layerCount = static_cast<uint32_t>(gliTex.layers());
	upload.m_stagingBuffer = stagingBuffer;
	upload.m_texture = *image;

	// copy image data to staging buffer
	upload.m_bufferCopyRegions.reserve(gliTex.levels());
	{
		uint8_t *data;
		stagingBuffer->map((void **)&data);

		// keep track of current offset in staging buffer
		size_t currentOffset = 0;

		// if this is a compressed format, how many texels is each block?
		auto blockExtent = gli::block_extent(gliTex.format());

		for (size_t level = 0; level < gliTex.levels(); ++level)
		{
			// blocks in this level; use max to ensure we dont divide e.g. extent = 2 by blockExtent = 4
			auto blockDims = glm::max(gliTex.extent(level), blockExtent) / blockExtent;
			// size of a texel row in bytes in the src data
			size_t rowSize = blockDims.x * gli::block_size(gliTex.format());
			// size of a row in the staging buffer
			size_t rowPitch = util::alignPow2Up(rowSize, (size_t)m_device->getBufferCopyRowPitchAlignment());
			assert(rowPitch % gli::block_size(gliTex.format()) == 0);

			// TODO: can we copy all layers and faces in one go?
			for (size_t layer = 0; layer < gliTex.layers(); ++layer)
			{
				for (size_t face = 0; face < gliTex.faces(); ++face)
				{
					// ensure each copy region starts at the proper alignment in the staging buffer
					currentOffset = util::alignPow2Up(currentOffset, (size_t)m_device->getBufferCopyOffsetAlignment());
					const uint8_t *srcData = (uint8_t *)gliTex.data(layer, face, level);

					gal::BufferImageCopy bufferCopyRegion{};
					bufferCopyRegion.m_imageMipLevel = static_cast<uint32_t>(level);
					bufferCopyRegion.m_imageBaseLayer = static_cast<uint32_t>(layer);
					bufferCopyRegion.m_imageLayerCount = 1;
					bufferCopyRegion.m_extent.m_width = static_cast<uint32_t>(gliTex.extent(level).x);
					bufferCopyRegion.m_extent.m_height = static_cast<uint32_t>(gliTex.extent(level).y);
					bufferCopyRegion.m_extent.m_depth = static_cast<uint32_t>(gliTex.extent(level).z);
					bufferCopyRegion.m_bufferOffset = currentOffset;
					bufferCopyRegion.m_bufferRowLength = (uint32_t)(rowPitch / gli::block_size(gliTex.format()) * gli::block_extent(gliTex.format()).x); // this is in pixels
					bufferCopyRegion.m_bufferImageHeight = static_cast<uint32_t>(glm::max(gliTex.extent(level), blockExtent).y);

					upload.m_bufferCopyRegions.push_back(bufferCopyRegion);

					for (size_t depth = 0; depth < blockDims.z; ++depth)
					{
						for (size_t row = 0; row < blockDims.y; ++row)
						{
							memcpy(data + currentOffset, srcData, rowSize);
							srcData += rowSize;
							currentOffset += rowPitch;
						}
					}
				}
			}
		}

		stagingBuffer->unmap();
	}

	{
		LOCK_HOLDER(m_uploadDataMutex);
		m_uploads.push_back(eastl::move(upload));
	}

	return true;
}

bool TextureLoader::loadRawRGBA8(size_t fileSize, const char *fileData, const char *textureName, uint32_t width, uint32_t height, gal::Image **image, gal::ImageView **imageView) noexcept
{
	if (width == 0 || height == 0)
	{
		Log::warn("Failed to load texture: \"%s\"", textureName);
		return false;
	}

	// create image
	{
		gal::ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = width;
		imageCreateInfo.m_height = height;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_samples = gal::SampleCount::_1;
		imageCreateInfo.m_imageType = gal::ImageType::_2D;
		imageCreateInfo.m_format = gal::Format::R8G8B8A8_UNORM;
		imageCreateInfo.m_createFlags = {};
		imageCreateInfo.m_usageFlags = gal::ImageUsageFlags::TEXTURE_BIT | gal::ImageUsageFlags::TRANSFER_DST_BIT;
		imageCreateInfo.m_optimizedClearValue = {};

		m_device->createImage(imageCreateInfo, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, {}, false, image);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE, *image, textureName);

		// create image view
		m_device->createImageView(*image, imageView);
		m_device->setDebugObjectName(gal::ObjectType::IMAGE_VIEW, *imageView, textureName);
	}

	// create staging buffer
	gal::Buffer *stagingBuffer = nullptr;
	{
		gal::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = fileSize * 2; // account for padding. TODO: calculate actual size requirement
		bufferCreateInfo.m_usageFlags = gal::BufferUsageFlags::TRANSFER_SRC_BIT;

		m_device->createBuffer(bufferCreateInfo, gal::MemoryPropertyFlags::HOST_COHERENT_BIT | gal::MemoryPropertyFlags::HOST_VISIBLE_BIT, {}, false, &stagingBuffer);
	}

	Upload upload = {};
	upload.m_imageSubresourceRange.m_baseMipLevel = 0;
	upload.m_imageSubresourceRange.m_levelCount = 1;
	upload.m_imageSubresourceRange.m_baseArrayLayer = 0;
	upload.m_imageSubresourceRange.m_layerCount = 1;
	upload.m_stagingBuffer = stagingBuffer;
	upload.m_texture = *image;

	// copy image data to staging buffer
	upload.m_bufferCopyRegions.reserve(1);
	{
		uint8_t *data;
		stagingBuffer->map((void **)&data);

		// keep track of current offset in staging buffer
		size_t currentOffset = 0;

		{
			// size of a texel row in bytes in the src data
			size_t rowSize = width * 4;
			// size of a row in the staging buffer
			size_t rowPitch = util::alignPow2Up(rowSize, (size_t)m_device->getBufferCopyRowPitchAlignment());

			// ensure each copy region starts at the proper alignment in the staging buffer
			currentOffset = util::alignPow2Up(currentOffset, (size_t)m_device->getBufferCopyOffsetAlignment());
			const uint8_t *srcData = (uint8_t *)fileData;

			gal::BufferImageCopy bufferCopyRegion{};
			bufferCopyRegion.m_imageMipLevel = 0;
			bufferCopyRegion.m_imageBaseLayer = 0;
			bufferCopyRegion.m_imageLayerCount = 1;
			bufferCopyRegion.m_extent.m_width = width;
			bufferCopyRegion.m_extent.m_height = height;
			bufferCopyRegion.m_extent.m_depth = 1;
			bufferCopyRegion.m_bufferOffset = currentOffset;
			bufferCopyRegion.m_bufferRowLength = (uint32_t)(rowPitch / 4); // this is in pixels
			bufferCopyRegion.m_bufferImageHeight = height;

			upload.m_bufferCopyRegions.push_back(bufferCopyRegion);

			for (size_t row = 0; row < height; ++row)
			{
				memcpy(data + currentOffset, srcData, rowSize);
				srcData += rowSize;
				currentOffset += rowPitch;
			}
		}

		stagingBuffer->unmap();
	}

	{
		LOCK_HOLDER(m_uploadDataMutex);
		m_uploads.push_back(eastl::move(upload));
	}

	return true;
}

void TextureLoader::flushUploadCopies(gal::CommandList *cmdList, uint64_t frameIndex) noexcept
{
	// delete old staging buffers
	while (!m_deletionQueue.empty())
	{
		auto &usedBuffer = m_deletionQueue.front();

		if (usedBuffer.m_frameToFreeIn <= frameIndex)
		{
			m_device->destroyBuffer(usedBuffer.m_stagingBuffer);
			m_deletionQueue.pop();
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
	barriers.reserve(m_uploads.size());

	// transition from UNDEFINED to TRANSFER_DST
	{
		for (auto &upload : m_uploads)
		{
			barriers.push_back(gal::Initializers::imageBarrier(upload.m_texture,
				gal::PipelineStageFlags::TOP_OF_PIPE_BIT,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::ResourceState::UNDEFINED,
				gal::ResourceState::WRITE_TRANSFER,
				upload.m_imageSubresourceRange));
		}

		cmdList->barrier((uint32_t)barriers.size(), barriers.data());
	}

	// upload data
	for (auto &upload : m_uploads)
	{
		cmdList->copyBufferToImage(upload.m_stagingBuffer, upload.m_texture, static_cast<uint32_t>(upload.m_bufferCopyRegions.size()), upload.m_bufferCopyRegions.data());

		// delete staging buffer in 2 frames from now
		m_deletionQueue.push({ upload.m_stagingBuffer, (frameIndex + 2) });
	}

	// transition form TRANSFER_DST to TEXTURE
	{
		barriers.clear();
		for (auto &upload : m_uploads)
		{
			barriers.push_back(gal::Initializers::imageBarrier(upload.m_texture,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::PipelineStageFlags::VERTEX_SHADER_BIT | gal::PipelineStageFlags::PIXEL_SHADER_BIT | gal::PipelineStageFlags::COMPUTE_SHADER_BIT,
				gal::ResourceState::WRITE_TRANSFER,
				gal::ResourceState::READ_RESOURCE,
				upload.m_imageSubresourceRange));
		}

		cmdList->barrier((uint32_t)barriers.size(), barriers.data());
	}

	m_uploads.clear();
}
