#pragma once
#include "utility/HandleManager.h"
#include <EASTL/vector.h>
#include "gal/FwdDecl.h"
#include "ViewHandles.h"
#include "Handles.h"

class ResourceViewRegistry;

/// <summary>
/// This class is responsible for managing TextureHandles which map to Image, ImageView and TextureViewHandle objects.
/// </summary>
class TextureManager
{
public:
	explicit TextureManager(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry) noexcept;
	TextureHandle add(gal::Image *image, gal::ImageView *view) noexcept;
	void update(TextureHandle handle, gal::Image *image, gal::ImageView *view) noexcept;
	void free(TextureHandle handle) noexcept;
	TextureViewHandle getViewHandle(TextureHandle handle) const noexcept;

private:
	struct Texture
	{
		gal::Image *m_image = nullptr;
		gal::ImageView *m_view = nullptr;
		TextureViewHandle m_viewHandle = {};
	};

	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	HandleManager m_handleManager;
	eastl::vector<Texture> m_textures;
};