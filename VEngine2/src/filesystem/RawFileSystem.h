#pragma once
#include <EASTL/vector.h>
#include <filesystem>
#include "IFileSystem.h"
#include "utility/HandleManager.h"
#include "utility/SpinLock.h"

class RawFileSystem : public IFileSystem
{
public:
	static RawFileSystem &get() noexcept;

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
	struct OpenFile
	{
		char m_path[k_maxPathLength];
		FILE *m_file;
	};

	struct FileFind
	{
		std::filesystem::directory_iterator m_iterator;
	};

	eastl::vector<OpenFile> m_openFiles;
	eastl::vector<FileFind> m_fileFinds;
	HandleManager m_openFileHandleManager;
	HandleManager m_fileFindHandleManager;
	mutable SpinLock m_openFilesSpinLock;
	mutable SpinLock m_fileFindsSpinLock;

	explicit RawFileSystem() noexcept = default;
	std::filesystem::directory_iterator advanceDirIterator(std::filesystem::directory_iterator it, const char *initialPath) noexcept;
};