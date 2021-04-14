#include "TextureManager.h"
#include "Log.h"
#include "ResourceViewRegistry.h"

TextureManager::TextureManager(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry)
{
	m_textures.resize(16);
}

TextureHandle TextureManager::add(gal::Image *image, gal::ImageView *view) noexcept
{
	uint32_t handle = m_handleManager.allocate();

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
		m_handleManager.free(handle);
		return TextureHandle();
	}

	if (handle > m_textures.size())
	{
		m_textures.resize((size_t)(m_textures.size() * 1.5));
	}

	auto &tex = m_textures[handle - 1];
	tex = {};
	tex.m_image = image;
	tex.m_view = view;
	tex.m_viewHandle = viewHandle;
	

	return TextureHandle(handle);
}

void TextureManager::update(TextureHandle handle, gal::Image *image, gal::ImageView *view) noexcept
{
	if (handle == 0 || handle > m_textures.size())
	{
		Log::warn("TextureManager: Tried to update an invalid TextureHandle!");
		return;
	}

	auto &tex = m_textures[handle - 1];
	tex = {};
	tex.m_image = image;
	tex.m_view = view;

	m_viewRegistry->updateHandle(tex.m_viewHandle, view);
}

void TextureManager::free(TextureHandle handle) noexcept
{
	if (handle == 0 || handle > m_textures.size())
	{
		return;
	}

	auto &tex = m_textures[handle - 1];

	m_viewRegistry->destroyHandle(tex.m_viewHandle);

	if (tex.m_image)
	{
		m_device->destroyImage(tex.m_image);
		m_device->destroyImageView(tex.m_view);
	}

	tex = {};
	m_handleManager.free(handle);
}

TextureViewHandle TextureManager::getViewHandle(TextureHandle handle) const noexcept
{
	if (handle == 0 || handle > m_textures.size())
	{
		return TextureViewHandle();
	}
	return m_textures[handle - 1].m_viewHandle;
}
