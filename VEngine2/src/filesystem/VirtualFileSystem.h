#pragma once
#include "IFileSystem.h"
#include <EASTL/vector.h>
#include <EASTL/string_map.h>

class VirtualFileSystem : public IFileSystem
{
public:
	static VirtualFileSystem &get() noexcept;

	bool mount(const char *nativePath, const char *mountName) noexcept;
	void unmount(const char *mountName) noexcept;
	void unmountAll() noexcept;
	bool resolve(const char *path, char *result) const noexcept;
	bool unresolve(const char *path, char *result) const noexcept;

	bool exists(const char *path) const noexcept override;
	bool isDirectory(const char *path) const noexcept override;
	bool isFile(const char *path) const noexcept override;
	bool createDirectoryHierarchy(const char *path) const noexcept override;
	bool rename(const char *path, const char *newName) const noexcept override;
	bool remove(const char *path) const noexcept override;

	FileHandle open(const char *filePath, FileMode mode, bool binary) noexcept override;
	uint64_t size(FileHandle fileHandle) const noexcept override;
	uint64_t size(const char *filePath) const noexcept override;
	uint64_t read(FileHandle fileHandle, size_t bufferSize, void *buffer) const noexcept override;
	uint64_t write(FileHandle fileHandle, size_t bufferSize, const void *buffer) const noexcept override;
	void close(FileHandle fileHandle) noexcept override;

	bool readFile(const char *filePath, size_t bufferSize, void *buffer, bool binary) noexcept override;
	bool writeFile(const char *filePath, size_t bufferSize, const void *buffer, bool binary) noexcept override;

	FileFindHandle findFirst(const char *dirPath, FileFindData *result) noexcept override;
	bool findNext(FileFindHandle findHandle, FileFindData *result) noexcept override;
	void findClose(FileFindHandle findHandle) noexcept override;

	FileSystemWatcherHandle openFileSystemWatcher(const char *path, FileSystemWatcherCallback callback, void *userData) noexcept override;
	void closeFileSystemWatcher(FileSystemWatcherHandle watcherHandle) noexcept override;

	TextureHandle getIcon(const char *path, Renderer *renderer, uint32_t *preferredWidth, uint32_t *preferredHeight) noexcept override;

private:
	eastl::string_map<eastl::vector<eastl::string8>> m_mountPoints;

	explicit VirtualFileSystem() noexcept = default;
};