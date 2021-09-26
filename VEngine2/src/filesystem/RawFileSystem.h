#pragma once
#include <EASTL/vector.h>
#include "IFileSystem.h"
#include "utility/HandleManager.h"
#include "utility/SpinLock.h"

class RawFileSystem : public IFileSystem
{
public:
	struct FileSystemWatcherThreadData
	{
		char m_path[IFileSystem::k_maxPathLength];
		size_t m_pathLen;
		char *m_buffer;
		size_t m_bufferSize;
		eastl::atomic_flag m_keepRunning;
		void *m_threadhandle;
		void *m_watchDirectoryHandle;
		IFileSystem::FileSystemWatcherCallback m_userCallback;
		void *m_userCallbackUserData;
	};

	static RawFileSystem &get() noexcept;

	void getCurrentPath(char *path) const noexcept;

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

	static constexpr size_t k_maxFileSystemWatchers = 64;

	struct OpenFile
	{
		char m_path[k_maxPathLength];
		void *m_file;
	};

	struct FileFind
	{
		char m_searchPath[k_maxPathLength];
		void *m_handle;
	};

	eastl::vector<OpenFile> m_openFiles;
	eastl::vector<FileFind> m_fileFinds;
	FileSystemWatcherThreadData m_fileSystemWatcherThreadData[k_maxFileSystemWatchers];
	HandleManager m_openFileHandleManager;
	HandleManager m_fileFindHandleManager;
	HandleManager m_fileSystemWatcherHandleManager;
	mutable SpinLock m_openFilesSpinLock;
	mutable SpinLock m_fileFindsSpinLock;
	mutable SpinLock m_fileSystemWatchersSpinLock;
	mutable SpinLock m_iconCacheSpinLock;

	explicit RawFileSystem() noexcept;
};