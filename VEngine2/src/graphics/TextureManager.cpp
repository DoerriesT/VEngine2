#include "TextureManager.h"
#include "Log.h"
#include "ResourceViewRegistry.h"

TextureManager::TextureManager(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry)
{
	m_textures.resize(16);
}

TextureManager::~TextureManager() noexcept
{
	for (auto &tex : m_textures)
	{
		if (tex.m_image)
		{
			m_device->destroyImageView(tex.m_view);
			m_device->destroyImage(tex.m_image);
		}

		if (tex.m_viewHandle)
		{
			m_viewRegistry->destroyHandle(tex.m_viewHandle);
		}
	}

	// empty the complete deletion queue
	while (!m_deletionQueue.empty())
	{
		auto &freedTexture = m_deletionQueue.front();

		m_device->destroyImageView(freedTexture.m_view);
		m_device->destroyImage(freedTexture.m_image);
		m_deletionQueue.pop();
	}
}

void TextureManager::flushDeletionQueue(uint64_t frameIndex) noexcept
{
	LOCK_HOLDER(m_deletionQueueMutex);

	while (!m_deletionQueue.empty())
	{
		auto &freedTexture = m_deletionQueue.front();

		if (freedTexture.m_frameToFreeIn <= frameIndex)
		{
			m_device->destroyImageView(freedTexture.m_view);
			m_device->destroyImage(freedTexture.m_image);
			m_deletionQueue.pop();
		}
		else
		{
			// since frameIndex is monotonically increasing, we can just break out of the loop once we encounter
			// the first item where we have not reached the frame to delete it in yet.
			break;
		}
	}
}

TextureHandle TextureManager::add(gal::Image *image, gal::ImageView *view) noexcept
{
	uint32_t handle = 0;
	{
		LOCK_HOLDER(m_handleManagerMutex);
		handle = m_handleManager.allocate();
	}

	// allocation failed
	if (handle == 0)
	{
		Log::err("TextureManager: Failed to allocate TextureHandle!");
		return TextureHandle();
	}

	TextureViewHandle viewHandle = m_viewRegistry->createTextureViewHandle(view);

	// allocation failed
	if (viewHandle == 0)
	{
		Log::err("TextureManager: Failed to allocate TextureViewHandle!");

		{
			LOCK_HOLDER(m_handleManagerMutex);
			m_handleManager.free(handle);
		}

		return TextureHandle();
	}

	{
		LOCK_HOLDER(m_texturesMutex);
		if (handle >= m_textures.size())
		{
			m_textures.resize((size_t)(m_textures.size() * 1.5));
		}

		auto &tex = m_textures[handle];
		tex = {};
		tex.m_image = image;
		tex.m_view = view;
		tex.m_viewHandle = viewHandle;
	}

	return TextureHandle(handle);
}

void TextureManager::update(TextureHandle handle, gal::Image *image, gal::ImageView *view) noexcept
{
	TextureViewHandle viewHandle = {};
	{
		LOCK_HOLDER(m_texturesMutex);
		const bool validHandle = handle != 0 && handle < m_textures.size();
		if (!validHandle)
		{
			Log::warn("TextureManager: Tried to update an invalid TextureHandle!");
			return;
		}

		auto &tex = m_textures[handle];
		tex = {};
		tex.m_image = image;
		tex.m_view = view;

		viewHandle = tex.m_viewHandle;
	}

	m_viewRegistry->updateHandle(viewHandle, view);
}

void TextureManager::free(TextureHandle handle, uint64_t frameIndex) noexcept
{
	Texture tex;
	{
		LOCK_HOLDER(m_texturesMutex);
		const bool validHandle = handle != 0 && handle < m_textures.size();
		if (!validHandle)
		{
			return;
		}

		tex = m_textures[handle];
		m_textures[handle] = {};
	}
	
	m_viewRegistry->destroyHandle(tex.m_viewHandle);

	if (tex.m_image)
	{
		LOCK_HOLDER(m_deletionQueueMutex);

		// delete texture in 2 frames from now
		m_deletionQueue.push({ tex.m_image, tex.m_view, (frameIndex + 2) });
	}

	{
		LOCK_HOLDER(m_handleManagerMutex);
		m_handleManager.free(handle);
	}
	
}

TextureViewHandle TextureManager::getViewHandle(TextureHandle handle) const noexcept
{
	LOCK_HOLDER(m_texturesMutex);
	const bool validHandle = handle != 0 && handle < m_textures.size();
	if (!validHandle)
	{
		return TextureViewHandle();
	}
	return m_textures[handle].m_viewHandle;
}