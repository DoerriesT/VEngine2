#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include "utility/SpinLock.h"

/// <summary>
/// This class is responsible for loading textures from files into GPU memory and creating corresponding
/// Image and ImageView objects.
/// </summary>
class TextureLoader
{
public:
	explicit TextureLoader(gal::GraphicsDevice *device) noexcept;
	~TextureLoader() noexcept;

	/// <summary>
	/// Loads a texture from memory into GPU memory and creates a corresponding Image and ImageView.
	/// flushUploadCopies() needs to be called before the texture can be used.
	/// This function is internally synchronized.
	/// </summary>
	/// <param name="fileSize">The size in bytes of the texture file.</param>
	/// <param name="fileData">The texture file data. Must be a DDS file.</param>
	/// <param name="textureName">A (non-unique) name to refer to the texture in logs and debug tools.</param>
	/// <param name="image">[Out] A pointer to the resulting Image.</param>
	/// <param name="imageView">[Out] A pointer to the resulting ImageView.</param>
	/// <returns>True if the call succeeded.</returns>
	bool load(size_t fileSize, const char *fileData, const char *textureName, gal::Image **image, gal::ImageView **imageView) noexcept;

	/// <summary>
	/// Uses the given command list to copy pending textures from their respective staging buffer to their final
	/// location in device memory. This function must be called after using load() before the texture can be used by the GPU.
	/// This function must not be called from multiple threads simultaneously.
	/// </summary>
	/// <param name="cmdList">The command list to record the copy commands to.</param>
	/// <param name="frameIndex">The current frame index. This is used internally to free old staging buffers.</param>
	/// <returns></returns>
	void flushUploadCopies(gal::CommandList *cmdList, uint64_t frameIndex) noexcept;

private:
	struct Upload
	{
		gal::ImageSubresourceRange m_imageSubresourceRange = {};
		gal::Buffer *m_stagingBuffer = nullptr;
		gal::Image *m_texture = nullptr;
		eastl::vector<gal::BufferImageCopy> m_bufferCopyRegions;
	};

	struct UsedStagingBufer
	{
		gal::Buffer *m_stagingBuffer = nullptr;
		uint64_t m_frameToFreeIn = UINT64_MAX;
	};

	SpinLock m_uploadDataMutex;
	gal::GraphicsDevice *m_device = nullptr;
	eastl::vector<Upload> m_uploads;
	eastl::queue<UsedStagingBufer> m_deletionQueue;
};