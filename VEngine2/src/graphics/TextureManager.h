#pragma once
#include "utility/HandleManager.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include "gal/FwdDecl.h"
#include "ViewHandles.h"
#include "Handles.h"
#include "utility/SpinLock.h"

class ResourceViewRegistry;

/// <summary>
/// This class is responsible for managing TextureHandles which map to Image, ImageView and TextureViewHandle objects.
/// TextureHandle is a high level concept to refer to a texture. In contrast, TextureViewHandle can be cast to uint32 and
/// used on the GPU to index into the texture descriptor array. The purpose if this class is to decouple the high level user
/// from the low level TextureViewHandle and add optional automatic resource destruction.
/// </summary>
class TextureManager
{
public:
	explicit TextureManager(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry) noexcept;
	~TextureManager() noexcept;

	/// <summary>
	/// Destroys old textures that were added to an internal deletion queue during a call to free().
	/// This should be called once per frame. A good place is right after waiting on the GPU frame Semaphore.
	/// This function is internally synchronized.
	/// </summary>
	/// <param name="frameIndex">The index of the current frame.</param>
	/// <returns></returns>
	void flushDeletionQueue(uint64_t frameIndex) noexcept;

	/// <summary>
	/// Adds an existing Image and ImageView to the TextureManager.
	/// The TextureManager will register a TextureViewHandle with the ResourceViewRegistry that can be
	/// used to access the texture from the GPU. When free() is called, the TextureManager will free the
	/// TextureViewHandle from the ResourceViewRegistry and destroy the Image and ImageView. Destruction
	/// of Image and ImageView can be disabled by passing a null Image instead. The user is then responsible
	/// for destroying the Image and ImageView. This function is internally synchronized.
	/// </summary>
	/// <param name="image">The Image object of the texture to add. Can be null if the user wants to manually destroy the Image and ImageView objects.</param>
	/// <param name="view">The ImageView object of the texture to add.</param>
	/// <returns>A valid TextureHandle referring to a TextureViewHandle of the texture or a null handle if the call failed.</returns>
	TextureHandle add(gal::Image *image, gal::ImageView *view) noexcept;

	/// <summary>
	/// Updates a TextureHandle that was created by a previous call to add() to point to a new ImageView.
	/// The TextureManager will register a TextureViewHandle with the ResourceViewRegistry that can be
	/// used to access the texture from the GPU. When free() is called, the TextureManager will free the
	/// TextureViewHandle from the ResourceViewRegistry and destroy the Image and ImageView. Destruction
	/// of Image and ImageView can be disabled by passing a null Image instead. The user is then responsible
	/// for destroying the Image and ImageView. This function is internally synchronized.
	/// </summary>
	/// <param name="handle">The TextureHandle to update.</param>
	/// <param name="image">The Image object of the texture to add. Can be null if the user wants to manually destroy the Image and ImageView objects.</param>
	/// <param name="view">The ImageView object of the texture to add.</param>
	/// <returns></returns>
	void update(TextureHandle handle, gal::Image *image, gal::ImageView *view) noexcept;

	/// <summary>
	/// Frees a TextureHandle that was created by a previous call to add().
	/// Freeing a texture frees the underlying TextureViewHandle allocated from the ResourceViewRegistry and optionally destroys the Image and ImageView.
	/// This function is internally synchronized.
	/// </summary>
	/// <param name="handle">The TextureHandle to free.</param>
	/// <param name="frameIndex">The current frame index. This is used internally to defer resource destruction.</param>
	/// <returns></returns>
	void free(TextureHandle handle, uint64_t frameIndex) noexcept;

	/// <summary>
	/// Gets the underlying TextureViewHandle (allocated from the ResourceViewRegistry) that corresponds to this texture.
	/// This function is internally synchronized.
	/// </summary>
	/// <param name="handle">The TextureHandle to get the TextureViewHandle for.</param>
	/// <returns>A valid TextureViewHandle or a null handle if the given TextureHandle is invalid.</returns>
	TextureViewHandle getViewHandle(TextureHandle handle) const noexcept;

private:
	struct Texture
	{
		gal::Image *m_image = nullptr;
		gal::ImageView *m_view = nullptr;
		TextureViewHandle m_viewHandle = {};
	};

	struct FreedTexture
	{
		gal::Image *m_image = nullptr;
		gal::ImageView *m_view = nullptr;
		uint64_t m_frameToFreeIn = UINT64_MAX;
	};

	mutable SpinLock m_handleManagerMutex;
	mutable SpinLock m_texturesMutex;
	mutable SpinLock m_deletionQueueMutex;
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	HandleManager m_handleManager;
	eastl::vector<Texture> m_textures;
	eastl::queue<FreedTexture> m_deletionQueue;
};