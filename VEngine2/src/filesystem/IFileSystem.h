#pragma once
#include <stdint.h>
#include "Handles.h"

enum FileHandle : size_t { NULL_FILE_HANDLE };
enum FileFindHandle : size_t { NULL_FILE_FIND_HANDLE };
enum FileSystemWatcherHandle : size_t { NULL_FILE_SYSTEM_WATCHER_HANDLE };

struct FileFindData;
class Renderer;

enum class FileMode
{
	/// <summary>
	/// Open file for input operations. The file must exist.
	/// </summary>
	READ,

	/// <summary>
	/// Create an empty file for output operations. 
	/// If a file with the same name already exists, its contents are discarded 
	/// and the file is treated as a new empty file. 
	/// </summary>
	WRITE,

	/// <summary>
	/// Open file for output at the end of a file. Output operations always write 
	/// data at the end of the file, expanding it.
	/// Repositioning operations (fseek, fsetpos, rewind) are ignored. 
	/// The file is created if it does not exist.
	/// </summary>
	APPEND,

	/// <summary>
	/// Open a file for update (both for input and output). The file must exist.
	/// </summary>
	OPEN_READ_WRITE,

	/// <summary>
	/// Create an empty file and open it for update (both for input and output). 
	/// If a file with the same name already exists its contents are discarded 
	/// and the file is treated as a new empty file.
	/// </summary>
	CREATE_READ_WRITE,

	/// <summary>
	/// Open a file for update (both for input and output) with all output operations 
	/// writing data at the end of the file. 
	/// Repositioning operations (fseek, fsetpos, rewind) affects the next input operations, 
	/// but output operations move the position back to the end of file. 
	/// The file is created if it does not exist.
	/// </summary>
	APPEND_OR_CREATE_READ_WRITE
};

enum class FileChangeType
{
	/// <summary>
	/// The file was added to the directory.
	/// </summary>
	ADDED,

	/// <summary>
	/// The file was removed from the directory.
	/// </summary>
	REMOVED,

	/// <summary>
	/// The file was modified. This can be a change in the time stamp or attributes
	/// </summary>
	MODIFIED,

	/// <summary>
	/// The file was renamed and this is the old name.
	/// </summary>
	RENAMED_OLD_NAME,

	/// <summary>
	/// The file was renamed and this is the new name.
	/// </summary>
	RENAMED_NEW_NAME
};

struct FileFindData
{
	char m_path[260];
	bool m_isFile;
	bool m_isDirectory;
};

class IFileSystem
{
public:
	static constexpr size_t k_maxPathLength = 260; // including null terminator

	using FileSystemWatcherCallback = void(*)(const char *path, FileChangeType changeType, void *userData);

	virtual bool exists(const char *path) const noexcept = 0;
	virtual bool isDirectory(const char *path) const noexcept = 0;
	virtual bool isFile(const char *path) const noexcept = 0;
	virtual bool createDirectoryHierarchy(const char *path) const noexcept = 0;
	virtual bool rename(const char *path, const char *newName) const noexcept = 0;
	virtual bool remove(const char *path) const noexcept = 0;


	virtual FileHandle open(const char *filePath, FileMode mode, bool binary) noexcept = 0;
	virtual uint64_t size(FileHandle fileHandle) const noexcept = 0;
	virtual uint64_t size(const char *filePath) const noexcept = 0;
	virtual uint64_t read(FileHandle fileHandle, size_t bufferSize, void *buffer) const noexcept = 0;
	virtual uint64_t write(FileHandle fileHandle, size_t bufferSize, const void *buffer) const noexcept = 0;
	virtual void close(FileHandle fileHandle) noexcept = 0;

	virtual bool readFile(const char *filePath, size_t bufferSize, void *buffer, bool binary) noexcept = 0;
	virtual bool writeFile(const char *filePath, size_t bufferSize, const void *buffer, bool binary) noexcept = 0;

	virtual FileFindHandle findFirst(const char *dirPath, FileFindData *result) noexcept = 0;
	virtual bool findNext(FileFindHandle findHandle, FileFindData *result) noexcept = 0;
	virtual void findClose(FileFindHandle findHandle) noexcept = 0;

	virtual FileSystemWatcherHandle openFileSystemWatcher(const char *path, FileSystemWatcherCallback callback, void *userData) noexcept = 0;
	virtual void closeFileSystemWatcher(FileSystemWatcherHandle watcherHandle) noexcept = 0;

	virtual TextureHandle getIcon(const char *path, Renderer *renderer, uint32_t *preferredWidth, uint32_t *preferredHeight) noexcept = 0;

	template<typename T>
	void iterate(const char *dirPath, T &&func) noexcept
	{
		FileFindData ffd;
		auto h = findFirst(dirPath, &ffd);
		if (h)
		{
			bool cont = false;
			do
			{
				cont = func(ffd);
			} while (cont && findNext(h, &ffd));
			findClose(h);
		}
	}

	template<typename T>
	void iterateRecursive(const char *dirPath, T &&func) noexcept
	{
		iterateRecursiveInternal(dirPath, func);
	}

private:
	template<typename T>
	bool iterateRecursiveInternal(const char *dirPath, T &&func) noexcept
	{
		FileFindData ffd;
		auto h = findFirst(dirPath, &ffd);
		if (h)
		{
			bool cont = false;
			do
			{
				cont = func(ffd);
				if (cont && ffd.m_isDirectory)
				{
					cont = iterateRecursiveInternal(ffd.m_path, func);
				}
			} while (cont && findNext(h, &ffd));
			findClose(h);
			
			return cont;
		}
		else
		{
			// not finding anything does not mean that the search should be aborted
			return true;
		}
	}
};