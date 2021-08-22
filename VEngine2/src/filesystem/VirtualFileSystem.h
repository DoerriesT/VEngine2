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

	FileHandle open(const char *filePath, FileMode mode, bool binary) noexcept override;
	uint64_t size(FileHandle fileHandle) const noexcept override;
	uint64_t size(const char *filePath) const noexcept override;
	uint64_t read(FileHandle fileHandle, size_t bufferSize, void *buffer) const noexcept override;
	uint64_t write(FileHandle fileHandle, size_t bufferSize, const void *buffer) const noexcept override;
	void close(FileHandle fileHandle) noexcept override;

	bool readFile(const char *filePath, size_t bufferSize, void *buffer) noexcept override;
	bool writeFile(const char *filePath, size_t bufferSize, const void *buffer) noexcept override;

	virtual FileFindHandle findFirst(const char *dirPath, char *resultPath) noexcept override;
	virtual FileFindHandle findNext(FileFindHandle findHandle, char *resultPath) noexcept override;
	bool findIsDirectory(FileFindHandle findHandle) const noexcept override;
	void findClose(FileFindHandle findHandle) noexcept override;

private:
	eastl::string_map<eastl::vector<eastl::string8>> m_mountPoints;

	explicit VirtualFileSystem() noexcept = default;
};