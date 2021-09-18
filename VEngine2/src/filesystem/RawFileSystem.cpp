#include "RawFileSystem.h"
#include "Log.h"
#include "utility/WideNarrowStringConversion.h"
#include <filesystem>
#include <assert.h>

#ifdef WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

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

#endif // WIN32

RawFileSystem &RawFileSystem::get() noexcept
{
	static RawFileSystem rfs;
	return rfs;
}

void RawFileSystem::getCurrentPath(char *path) const noexcept
{
	wchar_t tmp[k_maxPathLength];
	DWORD charsWritten = ::GetCurrentDirectoryW((DWORD)k_maxPathLength, tmp);

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

	if (!narrow(tmp, k_maxPathLength, path))
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
	std::error_code ec;
	bool val = std::filesystem::exists(path, ec);
	return val && !std::system_category().default_error_condition(ec.value());
}

bool RawFileSystem::isDirectory(const char *path) const noexcept
{
	std::error_code ec;
	bool val = std::filesystem::is_directory(path, ec);
	return val && !std::system_category().default_error_condition(ec.value());
}

bool RawFileSystem::isFile(const char *path) const noexcept
{
	std::error_code ec;
	bool val = std::filesystem::is_regular_file(path, ec);
	return val && !std::system_category().default_error_condition(ec.value());
}

bool RawFileSystem::createDirectoryHierarchy(const char *path) const noexcept
{
	std::error_code ec;
	std::filesystem::create_directories(path, ec);
	return !std::system_category().default_error_condition(ec.value());
}

bool RawFileSystem::rename(const char *path, const char *newName) const noexcept
{
	std::filesystem::path newPath = path;
	newPath = newPath.parent_path();
	newPath /= newName;

	std::error_code ec;
	std::filesystem::rename(path, newPath, ec);
	return !std::system_category().default_error_condition(ec.value());
}

bool RawFileSystem::remove(const char *path) const noexcept
{
	std::error_code ec;
	std::filesystem::remove(path, ec);
	return !std::system_category().default_error_condition(ec.value());
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
		std::error_code ec;
		auto size = std::filesystem::file_size(path, ec);
		return !std::system_category().default_error_condition(ec.value()) ? size : 0;
	}

	return 0;
}

uint64_t RawFileSystem::size(const char *filePath) const noexcept
{
	std::error_code ec;
	auto size = std::filesystem::file_size(filePath, ec);
	return !std::system_category().default_error_condition(ec.value()) ? size : 0;
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
#ifdef WIN32

	*result = {};

	// prepare input in utf8 form
	std::string utf8input;
	{
		size_t curOffset = 0;
		while (dirPath[curOffset])
		{
			if (dirPath[curOffset] == '/')
			{
				utf8input.push_back('\\');
			}
			else
			{
				utf8input.push_back(dirPath[curOffset]);
			}
			++curOffset;
		}

		utf8input.append("\\*");
	}

	WIN32_FIND_DATAW  win32FindData{};
	HANDLE win32Handle = FindFirstFileW(widen(utf8input.c_str()).c_str(), &win32FindData);

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

		auto foundPath = narrow(win32FindData.cFileName);
		strcpy_s(result->m_path, ff.m_searchPath);
		strcat_s(result->m_path, foundPath.c_str());
		getDirectoryFileFlags(win32FindData.dwFileAttributes, &result->m_isDirectory, &result->m_isFile);

		return resultHandle;
	}
	else
	{
		FindClose(win32Handle);
		return NULL_FILE_FIND_HANDLE;
	}

#else // WIN32
	static_assert(false);
#endif
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

#ifdef WIN32

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

		auto foundPath = narrow(win32FindData.cFileName);
		strcpy_s(result->m_path, ff.m_searchPath);
		strcat_s(result->m_path, foundPath.c_str());
		getDirectoryFileFlags(win32FindData.dwFileAttributes, &result->m_isDirectory, &result->m_isFile);
	}

	return res;

#else // WIN32
	static_assert(false);
#endif
}

void RawFileSystem::findClose(FileFindHandle findHandle) noexcept
{
	if (!findHandle)
	{
		return;
	}

	LOCK_HOLDER(m_fileFindsSpinLock);

#ifdef WIN32

	FindClose(m_fileFinds[findHandle - 1].m_handle);
	m_fileFinds[findHandle - 1].m_searchPath[0] = '\0';
	m_fileFinds[findHandle - 1].m_handle = INVALID_HANDLE_VALUE;
	m_fileFindHandleManager.free(findHandle);

#else // WIN32
	static_assert(false);
#endif
}

static void fileSystemWatcherOnNotify(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OVERLAPPED *lpOverlapped);

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
				strcat_s(threadUserData->m_path, IFileSystem::k_maxPathLength, narrow(tmpW).c_str());

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

FileSystemWatcherHandle RawFileSystem::openFileSystemWatcher(const char *path, FileSystemWatcherCallback callback, void *userData) noexcept
{
	// allocate buffer for directory change data
	const size_t bufferSize = 4096;
	char *buffer = new char[bufferSize];

	if (!buffer)
	{
		return NULL_FILE_SYSTEM_WATCHER_HANDLE;
	}

	// create handle for watched directory
	HANDLE dirHandle = ::CreateFileW(widen(path).c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
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

RawFileSystem::RawFileSystem() noexcept
	:m_fileSystemWatcherHandleManager(k_maxFileSystemWatchers)
{
}
