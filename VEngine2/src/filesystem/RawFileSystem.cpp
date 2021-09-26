#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "RawFileSystem.h"
#include <assert.h>
#include <string.h>
#include <Windows.h>
#include <EASTL/string_hash_map.h>
#include <EASTL/hash_map.h>
#include "Log.h"
#include "Path.h"
#include "utility/WideNarrowStringConversion.h"
#include "utility/Memory.h"
#include "utility/Utility.h"
#include "graphics/Renderer.h"
#include <ShObjIdl_core.h>

namespace
{
	static void fileSystemWatcherOnNotify(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OVERLAPPED *lpOverlapped);

	static void getDirectoryFileFlags(DWORD flags, bool *isDir, bool *isFile)
	{
		*isDir = false;
		*isFile = false;

		if (flags & FILE_ATTRIBUTE_HIDDEN)
		{
			return;
		}
		*isDir = flags & FILE_ATTRIBUTE_DIRECTORY;
		*isFile = flags & FILE_ATTRIBUTE_ARCHIVE;
	}

	static bool beginReadDirectoryChanges(OVERLAPPED *lpOverlapped)
	{
		RawFileSystem::FileSystemWatcherThreadData *threadUserData = (RawFileSystem::FileSystemWatcherThreadData *)lpOverlapped->hEvent;

		constexpr DWORD readDirChangeFiler =
			FILE_NOTIFY_CHANGE_CREATION |
			FILE_NOTIFY_CHANGE_LAST_WRITE |
			FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_FILE_NAME;

		DWORD bytesReturned = 0; // unused for async use

		BOOL status = ::ReadDirectoryChangesW(
			threadUserData->m_watchDirectoryHandle,
			threadUserData->m_buffer,
			(DWORD)threadUserData->m_bufferSize,
			true, // monitor children
			readDirChangeFiler,
			&bytesReturned,
			lpOverlapped,
			fileSystemWatcherOnNotify
		);

		return status;
	}

	static void fileSystemWatcherOnNotify(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OVERLAPPED *lpOverlapped)
	{
		RawFileSystem::FileSystemWatcherThreadData *threadUserData = (RawFileSystem::FileSystemWatcherThreadData *)lpOverlapped->hEvent;

		// the I/O operation was aborted; tell the thread to kill itself 
		if (dwErrorCode == ERROR_OPERATION_ABORTED)
		{
			threadUserData->m_keepRunning.clear();
			return;
		}

		if (dwNumberOfBytesTransfered != 0)
		{
			wchar_t tmpW[IFileSystem::k_maxPathLength];


			FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)threadUserData->m_buffer;
			while (info)
			{
				FileChangeType changeType{};
				bool validAction = true;

				switch (info->Action)
				{
				case FILE_ACTION_ADDED:
					changeType = FileChangeType::ADDED;
					break;
				case FILE_ACTION_REMOVED:
					changeType = FileChangeType::REMOVED;
					break;
				case FILE_ACTION_MODIFIED:
					changeType = FileChangeType::MODIFIED;
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
					changeType = FileChangeType::RENAMED_OLD_NAME;
					break;
				case FILE_ACTION_RENAMED_NEW_NAME:
					changeType = FileChangeType::RENAMED_NEW_NAME;
					break;
				default:
					validAction = false;
					break;
				}

				if (validAction)
				{
					// copy to local memory, correct separator and add null terminator
					assert((info->FileNameLength % sizeof(wchar_t)) == 0);
					size_t copyLen = eastl::min<size_t>(info->FileNameLength / sizeof(wchar_t), (IFileSystem::k_maxPathLength - 1));

					for (size_t i = 0; i < copyLen; ++i)
					{
						tmpW[i] = info->FileName[i] == L'\\' ? L'/' : info->FileName[i];
					}
					tmpW[copyLen] = L'\0';

					// concatenate to watch path
					bool narrowRes = narrow(tmpW, IFileSystem::k_maxPathLength - threadUserData->m_pathLen, threadUserData->m_path + threadUserData->m_pathLen);
					assert(narrowRes);

					// invoke user callback
					threadUserData->m_userCallback(threadUserData->m_path, changeType, threadUserData->m_userCallbackUserData);

					// reset watch path
					threadUserData->m_path[threadUserData->m_pathLen] = '\0';
				}

				info = info->NextEntryOffset == 0 ? nullptr : (FILE_NOTIFY_INFORMATION *)(((char *)info) + info->NextEntryOffset);
			}
		}

		// apparently you need to call the next ReadDirectoryChangesW from inside the completion routine
		beginReadDirectoryChanges(lpOverlapped);
	}

	static DWORD WINAPI fileSystemWatcherThreadFunc(_In_ void *lpParameter)
	{
		::SetThreadDescription(GetCurrentThread(), L"FileSystemWatcherThread");

		RawFileSystem::FileSystemWatcherThreadData *threadUserData = (RawFileSystem::FileSystemWatcherThreadData *)lpParameter;

		OVERLAPPED overlapped;
		::ZeroMemory(&overlapped, sizeof(overlapped));
		overlapped.hEvent = threadUserData; // hEvent is unused in the async + completion routine case, so we can store our own data here

		beginReadDirectoryChanges(&overlapped);

		while (threadUserData->m_keepRunning.test())
		{
			::SleepEx(INFINITE, true);
		}

		return EXIT_SUCCESS;
	}
}

RawFileSystem &RawFileSystem::get() noexcept
{
	static RawFileSystem rfs;
	return rfs;
}

void RawFileSystem::getCurrentPath(char *path) const noexcept
{
	// get required string lengths in characters
	wchar_t dummy = L'\0';
	DWORD requiredWStrLen = ::GetCurrentDirectoryW(1, &dummy);

	wchar_t *currentDirW = ALLOC_A_T(wchar_t, requiredWStrLen);

	DWORD charsWritten = ::GetCurrentDirectoryW(requiredWStrLen, currentDirW);

	// call failed or our buffer is not large enough
	if (charsWritten == 0 || charsWritten >= k_maxPathLength)
	{
		if (charsWritten >= k_maxPathLength)
		{
			Log::err("RawFileSystem::getCurrentPath(): Ran out of scratch memory.");
		}
		path[0] = '\0';
		return;
	}

	if (!narrow(currentDirW, k_maxPathLength, path))
	{
		Log::err("RawFileSystem::getCurrentPath(): Ran out of scratch memory.");
		path[0] = '\0';
		return;
	}

	// convert backwards to forwards slashes
	char *ptr = path;
	while (*ptr)
	{
		if (*ptr == '\\')
		{
			*ptr = '/';
		}

		++ptr;
	}
}

bool RawFileSystem::exists(const char *path) const noexcept
{
	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::exists(): Failed to widen() path!");
		return false;
	}

	DWORD attributes = ::GetFileAttributesW(pathW);

	return attributes != INVALID_FILE_ATTRIBUTES;
}

bool RawFileSystem::isDirectory(const char *path) const noexcept
{
	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::isDirectory(): Failed to widen() path!");
		return false;
	}

	DWORD attributes = ::GetFileAttributesW(pathW);

	return (attributes != INVALID_FILE_ATTRIBUTES)
		&& ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

bool RawFileSystem::isFile(const char *path) const noexcept
{
	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::isFile(): Failed to widen() path!");
		return false;
	}

	DWORD attributes = ::GetFileAttributesW(pathW);

	return (attributes != INVALID_FILE_ATTRIBUTES)
		&& ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool RawFileSystem::createDirectoryHierarchy(const char *path) const noexcept
{
	// widen input path
	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::rename(): Failed to widen() path!");
		return false;
	}

	wchar_t folder[MAX_PATH] = {};
	wchar_t *endOfStr = wcschr(pathW, L'\0');
	wchar_t *end = wcschr(pathW, L'/');
	end = end ? end : endOfStr;

	while (end)
	{
		wcsncpy_s(folder, pathW, end - pathW + 1);

		bool needToCreate = false;
		{
			DWORD attributes = ::GetFileAttributesW(folder);
			bool exists = attributes != INVALID_FILE_ATTRIBUTES;
			bool isDir = exists && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			needToCreate = !exists || (exists && !isDir);
		}

		if (needToCreate)
		{
			if (!::CreateDirectoryW(folder, nullptr))
			{
				DWORD err = GetLastError();

				if (err != ERROR_ALREADY_EXISTS)
				{
					Log::err("RawFileSystem::createDirectoryHierarchy(): Failed to create directory! Error: %u", (unsigned)err);
					return false;
				}
			}
		}

		if (end == endOfStr)
		{
			break;
		}

		++end;
		end = wcschr(end, L'/');
		end = end ? end : endOfStr;
	}

	return true;
}

bool RawFileSystem::rename(const char *path, const char *newName) const noexcept
{
	// widen input path
	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::rename(): Failed to widen() path!");
		return false;
	}

	// build new path from input path + new name
	char newPath[k_maxPathLength];
	strcpy_s(newPath, path);
	newPath[Path::getParentPath(newPath) + 1] = '\0';
	strcat_s(newPath, newName);

	// widen new path
	const size_t newPathLen = strlen(newPath);
	wchar_t *newPathW = ALLOC_A_T(wchar_t, newPathLen + 1);
	if (!widen(newPath, newPathLen + 1, newPathW))
	{
		Log::err("RawFileSystem::rename(): Failed to widen() new path!");
		return false;
	}

	return ::MoveFileW(pathW, newPathW);
}

bool RawFileSystem::remove(const char *path) const noexcept
{
	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::remove(): Failed to widen() path!");
		return false;
	}

	DWORD attributes = ::GetFileAttributesW(pathW);

	// file/directory does not exist
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		return false;
	}

	if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		return ::DeleteFileW(pathW);
	}
	else
	{
		return ::RemoveDirectoryW(pathW);
	}
}

FileHandle RawFileSystem::open(const char *filePath, FileMode mode, bool binary) noexcept
{
	const size_t filePathStrLen = strlen(filePath);
	if ((filePathStrLen + 1) > k_maxPathLength)
	{
		return NULL_FILE_HANDLE;
	}

	const char *fileMode = nullptr;

	switch (mode)
	{
	case FileMode::READ:
		fileMode = binary ? "rb" : "r";
		break;
	case FileMode::WRITE:
		fileMode = binary ? "wb" : "w";
		break;
	case FileMode::APPEND:
		fileMode = binary ? "ab" : "a";
		break;
	case FileMode::OPEN_READ_WRITE:
		fileMode = binary ? "r+b" : "r+";
		break;
	case FileMode::CREATE_READ_WRITE:
		fileMode = binary ? "w+b" : "w+";
		break;
	case FileMode::APPEND_OR_CREATE_READ_WRITE:
		fileMode = binary ? "a+b" : "a+";
		break;
	default:
		return NULL_FILE_HANDLE;
		break;
	}

	FILE *file = nullptr;
	errno_t err;

	if ((err = fopen_s(&file, filePath, fileMode)) != 0)
	{
		char buf[256];
		strerror_s(buf, err);
		Log::err("Failed to open file \"%s\": %s", filePath, buf);
		return NULL_FILE_HANDLE;
	}

	FileHandle resultHandle = {};
	{
		LOCK_HOLDER(m_openFilesSpinLock);
		resultHandle = (FileHandle)m_openFileHandleManager.allocate();

		if (!resultHandle)
		{
			fclose(file);
			return NULL_FILE_HANDLE;
		}

		OpenFile openFile{};
		memcpy(openFile.m_path, filePath, filePathStrLen + 1 /*null terminator*/);
		openFile.m_file = file;

		const size_t idx = (size_t)resultHandle - 1;

		if (m_openFiles.size() <= idx)
		{
			size_t newSize = idx;
			newSize += eastl::max<size_t>(1, newSize / 2);
			newSize = eastl::max<size_t>(16, newSize);
			m_openFiles.resize(newSize);
		}

		m_openFiles[idx] = openFile;
	}

	return resultHandle;
}

uint64_t RawFileSystem::size(FileHandle fileHandle) const noexcept
{
	if (!fileHandle)
	{
		return 0;
	}

	LOCK_HOLDER(m_openFilesSpinLock);

	const char *path = m_openFiles[fileHandle - 1].m_path;

	// path might be empty when the handle is invalid
	if (*path)
	{
		return size(path);
	}

	return 0;
}

uint64_t RawFileSystem::size(const char *filePath) const noexcept
{
	const size_t pathLen = strlen(filePath);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(filePath, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::isFile(): Failed to widen() path!");
		return false;
	}

	WIN32_FILE_ATTRIBUTE_DATA attributeData;
	if (!::GetFileAttributesExW(pathW, ::GetFileExInfoStandard, &attributeData))
	{
		return 0;
	}

	uint64_t result = 0;
	result |= attributeData.nFileSizeHigh;
	result <<= 32;
	result |= attributeData.nFileSizeLow;

	return result;
}

uint64_t RawFileSystem::read(FileHandle fileHandle, size_t bufferSize, void *buffer) const noexcept
{
	if (!fileHandle)
	{
		return 0;
	}

	LOCK_HOLDER(m_openFilesSpinLock);

	FILE *file = (FILE *)m_openFiles[fileHandle - 1].m_file;

	if (file)
	{
		return fread(buffer, 1, bufferSize, file);
	}

	return 0;
}

uint64_t RawFileSystem::write(FileHandle fileHandle, size_t bufferSize, const void *buffer) const noexcept
{
	if (!fileHandle)
	{
		return 0;
	}

	LOCK_HOLDER(m_openFilesSpinLock);

	FILE *file = (FILE *)m_openFiles[fileHandle - 1].m_file;

	if (file)
	{
		return fwrite(buffer, 1, bufferSize, file);
	}

	return 0;
}

void RawFileSystem::close(FileHandle fileHandle) noexcept
{
	if (!fileHandle)
	{
		return;
	}

	LOCK_HOLDER(m_openFilesSpinLock);

	FILE *file = (FILE *)m_openFiles[fileHandle - 1].m_file;

	if (file)
	{
		fclose(file);

		m_openFiles[fileHandle - 1] = {};

		m_openFileHandleManager.free((uint32_t)fileHandle);
	}
}

bool RawFileSystem::readFile(const char *filePath, size_t bufferSize, void *buffer, bool binary) noexcept
{
	auto fh = open(filePath, FileMode::READ, binary);
	if (fh)
	{
		uint64_t readCount = read(fh, bufferSize, buffer);
		close(fh);
		return readCount <= bufferSize;
	}
	return false;
}

bool RawFileSystem::writeFile(const char *filePath, size_t bufferSize, const void *buffer, bool binary) noexcept
{
	auto fh = open(filePath, FileMode::WRITE, binary);
	if (fh)
	{
		uint64_t writeCount = write(fh, bufferSize, buffer);
		close(fh);
		return writeCount == bufferSize;
	}
	return false;
}

FileFindHandle RawFileSystem::findFirst(const char *dirPath, FileFindData *result) noexcept
{
	*result = {};

	const char wildcardStrU8[] = "\\*";

	// prepare input in utf8 form
	const size_t utf8inputScratchMemSize = strlen(dirPath) + sizeof(wildcardStrU8);
	char *utf8input = ALLOC_A_T(char, utf8inputScratchMemSize);
	{
		size_t curOffset = 0;
		while (dirPath[curOffset])
		{
			if (dirPath[curOffset] == '/')
			{
				utf8input[curOffset] = '\\';
			}
			else
			{
				utf8input[curOffset] = dirPath[curOffset];
			}
			++curOffset;
		}
		utf8input[curOffset] = '\0';

		strcat_s(utf8input, utf8inputScratchMemSize, wildcardStrU8);
	}

	wchar_t *utf16input = ALLOC_A_T(wchar_t, utf8inputScratchMemSize);

	if (!widen(utf8input, utf8inputScratchMemSize, utf16input))
	{
		Log::err("RawFileSystem::findFirst(): Failed to widen() input path!");
		return NULL_FILE_FIND_HANDLE;
	}

	WIN32_FIND_DATAW  win32FindData{};
	HANDLE win32Handle = FindFirstFileW(utf16input, &win32FindData);

	if (win32Handle == INVALID_HANDLE_VALUE)
	{
		return NULL_FILE_FIND_HANDLE;
	}

	// skip . and .. entries
	bool res = true;
	do
	{
		const bool isDir = win32FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		const bool isDot = wcscmp(win32FindData.cFileName, L".") == 0;
		const bool isDotDot = wcscmp(win32FindData.cFileName, L"..") == 0;

		if (!(isDir && (isDot || isDotDot)))
		{
			break;
		}
	} while (res = FindNextFileW(win32Handle, &win32FindData));


	// if the directory is empty, we might have skipped through everything already, so check that we are still good
	if (res)
	{
		FileFind ff{};

		// allocate our handle
		FileFindHandle resultHandle = {};
		{
			LOCK_HOLDER(m_fileFindsSpinLock);
			resultHandle = (FileFindHandle)m_fileFindHandleManager.allocate();

			if (!resultHandle)
			{
				FindClose(win32Handle);
				return NULL_FILE_FIND_HANDLE;
			}

			size_t dirPathLen = strlen(dirPath);
			memcpy(ff.m_searchPath, dirPath, dirPathLen + 1 /*null terminator*/);
			ff.m_searchPath[dirPathLen] = '/';
			ff.m_searchPath[dirPathLen + 1] = '\0';
			ff.m_handle = win32Handle;

			const size_t idx = (size_t)resultHandle - 1;

			if (m_fileFinds.size() <= idx)
			{
				size_t newSize = idx;
				newSize += eastl::max<size_t>(1, newSize / 2);
				newSize = eastl::max<size_t>(16, newSize);
				m_fileFinds.resize(newSize);
			}

			m_fileFinds[idx] = ff;
		}

		const size_t searchPathLen = strlen(ff.m_searchPath);

		// first copy search path into result
		strcpy_s(result->m_path, ff.m_searchPath);
		// and then append the found file. note that we narrow() directly into the result buffer
		bool narrowRes = narrow(win32FindData.cFileName, k_maxPathLength - searchPathLen, result->m_path + searchPathLen);
		assert(narrowRes);

		getDirectoryFileFlags(win32FindData.dwFileAttributes, &result->m_isDirectory, &result->m_isFile);

		return resultHandle;
	}
	else
	{
		FindClose(win32Handle);
		return NULL_FILE_FIND_HANDLE;
	}
}

bool RawFileSystem::findNext(FileFindHandle findHandle, FileFindData *result) noexcept
{
	if (!findHandle)
	{
		return false;
	}

	FileFind ff{};
	{
		LOCK_HOLDER(m_fileFindsSpinLock);

		ff = m_fileFinds[findHandle - 1];
	}

	HANDLE win32Handle = ff.m_handle;
	assert(win32Handle != INVALID_HANDLE_VALUE);

	WIN32_FIND_DATAW  win32FindData{};
	bool res = FindNextFileW(win32Handle, &win32FindData);

	// skip . and .. entries
	do
	{
		const bool isDir = win32FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		const bool isDot = wcscmp(win32FindData.cFileName, L".") == 0;
		const bool isDotDot = wcscmp(win32FindData.cFileName, L"..") == 0;

		if (!(isDir && (isDot || isDotDot)))
		{
			break;
		}
	} while (res = FindNextFileW(win32Handle, &win32FindData));

	if (res)
	{
		*result = {};

		const size_t searchPathLen = strlen(ff.m_searchPath);

		// first copy search path into result
		strcpy_s(result->m_path, ff.m_searchPath);
		// and then append the found file. note that we narrow() directly into the result buffer
		bool narrowRes = narrow(win32FindData.cFileName, k_maxPathLength - searchPathLen, result->m_path + searchPathLen);
		assert(narrowRes);

		getDirectoryFileFlags(win32FindData.dwFileAttributes, &result->m_isDirectory, &result->m_isFile);
	}

	return res;
}

void RawFileSystem::findClose(FileFindHandle findHandle) noexcept
{
	if (!findHandle)
	{
		return;
	}

	LOCK_HOLDER(m_fileFindsSpinLock);

	FindClose(m_fileFinds[findHandle - 1].m_handle);
	m_fileFinds[findHandle - 1].m_searchPath[0] = '\0';
	m_fileFinds[findHandle - 1].m_handle = INVALID_HANDLE_VALUE;
	m_fileFindHandleManager.free(findHandle);
}

FileSystemWatcherHandle RawFileSystem::openFileSystemWatcher(const char *path, FileSystemWatcherCallback callback, void *userData) noexcept
{
	// allocate buffer for directory change data
	const size_t bufferSize = 4096;
	char *buffer = new char[bufferSize];

	if (!buffer)
	{
		return NULL_FILE_SYSTEM_WATCHER_HANDLE;
	}

	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::openFileSystemWatcher(): Failed to widen() path!");
		return NULL_FILE_SYSTEM_WATCHER_HANDLE;
	}

	// create handle for watched directory
	HANDLE dirHandle = ::CreateFileW(pathW, FILE_LIST_DIRECTORY, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
	if (dirHandle == INVALID_HANDLE_VALUE)
	{
		return NULL_FILE_SYSTEM_WATCHER_HANDLE;
	}

	// create handle of our watcher
	FileSystemWatcherHandle resultHandle = {};
	{
		LOCK_HOLDER(m_fileSystemWatchersSpinLock);
		resultHandle = (FileSystemWatcherHandle)m_fileSystemWatcherHandleManager.allocate();

		// failed to allocate handle
		if (!resultHandle)
		{
			::CloseHandle(dirHandle);
			return NULL_FILE_SYSTEM_WATCHER_HANDLE;
		}
	}

	const size_t idx = (size_t)resultHandle - 1;
	assert(!m_fileSystemWatcherThreadData[idx].m_keepRunning.test());

	strcpy_s(m_fileSystemWatcherThreadData[idx].m_path, path);
	strcat_s(m_fileSystemWatcherThreadData[idx].m_path, "/");
	m_fileSystemWatcherThreadData[idx].m_pathLen = strlen(m_fileSystemWatcherThreadData[idx].m_path);
	m_fileSystemWatcherThreadData[idx].m_buffer = buffer;
	m_fileSystemWatcherThreadData[idx].m_bufferSize = bufferSize;
	m_fileSystemWatcherThreadData[idx].m_keepRunning.test_and_set();
	m_fileSystemWatcherThreadData[idx].m_watchDirectoryHandle = dirHandle;
	m_fileSystemWatcherThreadData[idx].m_userCallback = callback;
	m_fileSystemWatcherThreadData[idx].m_userCallbackUserData = userData;
	m_fileSystemWatcherThreadData[idx].m_threadhandle = ::CreateThread(nullptr, 0, fileSystemWatcherThreadFunc, &m_fileSystemWatcherThreadData[idx], 0, nullptr);

	return resultHandle;
}

void RawFileSystem::closeFileSystemWatcher(FileSystemWatcherHandle watcherHandle) noexcept
{
	if (!watcherHandle)
	{
		return;
	}

	const size_t idx = (size_t)watcherHandle - 1;

	// reset atomic flag to signal to the thread to kill itself
	m_fileSystemWatcherThreadData[idx].m_keepRunning.clear();

	// wake up the thread
	auto dummyAPC = [](ULONG_PTR Parameter)
	{
		/*no need to do anything; we just want to wake the thread to check its flag*/
	};

	auto ret = ::QueueUserAPC(dummyAPC, m_fileSystemWatcherThreadData[idx].m_threadhandle, 0);
	assert(ret != 0);

	//bool bval = ::CancelIo(m_fileSystemWatcherThreadData[idx].m_watchDirectoryHandle);
	//assert(bval);

	// wait for the thread to actually finish before freeing its slot on the array
	ret = ::WaitForSingleObject(m_fileSystemWatcherThreadData[idx].m_threadhandle, INFINITE);
	assert(ret == WAIT_OBJECT_0);

	// close thread handle
	::CloseHandle(m_fileSystemWatcherThreadData[idx].m_threadhandle);
	m_fileSystemWatcherThreadData[idx].m_threadhandle = nullptr;

	// close directory handle
	::CloseHandle(m_fileSystemWatcherThreadData[idx].m_watchDirectoryHandle);
	m_fileSystemWatcherThreadData[idx].m_watchDirectoryHandle = nullptr;

	// free buffer
	delete[] m_fileSystemWatcherThreadData[idx].m_buffer;
	m_fileSystemWatcherThreadData[idx].m_buffer = nullptr;

	// now we can free our handle
	LOCK_HOLDER(m_fileSystemWatchersSpinLock);
	m_fileSystemWatcherHandleManager.free(watcherHandle);

}

TextureHandle RawFileSystem::getIcon(const char *path, Renderer *renderer, uint32_t *preferredWidth, uint32_t *preferredHeight) noexcept
{
	// maps from path to texture ID
	static eastl::string_hash_map<TextureHandle> iconMap;
	// maps from icon hash to texture ID
	static eastl::hash_map<size_t, TextureHandle> hasedIconMap;

	{
		LOCK_HOLDER(m_iconCacheSpinLock);

		auto it = iconMap.find(path);
		if (it != iconMap.end())
		{
			return it->second;
		}
	}
	

	TextureHandle resultTexID = {};

	const size_t pathLen = strlen(path);
	wchar_t *pathW = ALLOC_A_T(wchar_t, pathLen + 1);
	if (!widen(path, pathLen + 1, pathW))
	{
		Log::err("RawFileSystem::getIcon(): Failed to widen() path!");
		return NULL_TEXTURE_HANDLE;
	}

	for (size_t i = 0; pathW[i] != L'\0'; ++i)
	{
		if (pathW[i] == L'/')
		{
			pathW[i] = L'\\';
		}
	}

	// create IShellItemImageFactory from path
	IShellItemImageFactory *imageFactory = nullptr;
	auto hr = SHCreateItemFromParsingName(pathW, nullptr, __uuidof(IShellItemImageFactory), (void **)&imageFactory);

	if (SUCCEEDED(hr))
	{
		SIZE size = { (LONG)*preferredWidth, (LONG)*preferredHeight };

		//sz - Size of the image, SIIGBF_BIGGERSIZEOK - GetImage will stretch down the bitmap (preserving aspect ratio)
		HBITMAP hbmp;
		hr = imageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hbmp);
		if (SUCCEEDED(hr))
		{
			// query info about bitmap
			DIBSECTION ds;
			GetObjectW(hbmp, sizeof(ds), &ds);
			int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

			if (byteSize != 0)
			{
				// allocate memory and copy bitmap data to it
				uint8_t *data = new uint8_t[byteSize];
				GetBitmapBits(hbmp, byteSize, data);

				*preferredWidth = (uint32_t)ds.dsBm.bmWidth;
				*preferredWidth = (uint32_t)ds.dsBm.bmHeight;

				size_t hash = 0;

				// convert from BGRA to RGBA
				for (size_t i = 0; i < byteSize; i += 4)
				{
					auto tmp = data[i];
					data[i] = data[i + 2];
					data[i + 2] = tmp;

					util::hashCombine(hash, *(uint32_t *)(data + i));
				}

				{
					LOCK_HOLDER(m_iconCacheSpinLock);

					// found an identical image in our cache
					auto hashIt = hasedIconMap.find(hash);
					if (hashIt != hasedIconMap.end())
					{
						iconMap[path] = hashIt->second;
						resultTexID = hashIt->second;
					}
					else
					{
						// create texture and store in maps
						resultTexID = renderer->loadRawRGBA8(byteSize, (char *)data, path, ds.dsBm.bmWidth, ds.dsBm.bmHeight);
						iconMap[path] = resultTexID;
						hasedIconMap[hash] = resultTexID;
					}
				}
				

				// dont forget to free our allocation!
				delete[] data;
			}


			DeleteObject(hbmp);
		}

		imageFactory->Release();
	}

	return resultTexID;
}

RawFileSystem::RawFileSystem() noexcept
	:m_fileSystemWatcherHandleManager(k_maxFileSystemWatchers)
{
}
