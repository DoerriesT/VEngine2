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
	std::error_code ec;
	auto str = std::filesystem::current_path(ec).generic_u8string();
	if (!std::system_category().default_error_condition(ec.value()))
	{
		memcpy(path, str.c_str(), str.length() + 1);
	}
	else
	{
		path[0] = '\0';
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

bool RawFileSystem::readFile(const char *filePath, size_t bufferSize, void *buffer) noexcept
{
	auto fh = open(filePath, FileMode::READ, true);
	if (fh)
	{
		uint64_t readCount = read(fh, bufferSize, buffer);
		close(fh);
		return readCount <= bufferSize;
	}
	return false;
}

bool RawFileSystem::writeFile(const char *filePath, size_t bufferSize, const void *buffer) noexcept
{
	auto fh = open(filePath, FileMode::WRITE, true);
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