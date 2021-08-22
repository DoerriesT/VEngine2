#include "RawFileSystem.h"
#include "Log.h"

RawFileSystem &RawFileSystem::get() noexcept
{
	static RawFileSystem rfs;
	return rfs;
}

bool RawFileSystem::exists(const char *path) const noexcept
{
	return std::filesystem::exists(path);
}

bool RawFileSystem::isDirectory(const char *path) const noexcept
{
	return std::filesystem::is_directory(path);
}

bool RawFileSystem::isFile(const char *path) const noexcept
{
	return std::filesystem::is_regular_file(path);
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

	FILE *file = m_openFiles[fileHandle - 1].m_file;

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

	FILE *file = m_openFiles[fileHandle - 1].m_file;

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

	FILE *file = m_openFiles[fileHandle - 1].m_file;

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

FileFindHandle RawFileSystem::findFirst(const char *dirPath, char *resultPath) noexcept
{
	const size_t pathStrLen = strlen(dirPath);
	if ((pathStrLen + 1) > k_maxPathLength)
	{
		resultPath[0] = '\0';
		return NULL_FILE_FIND_HANDLE;
	}

	auto it = advanceDirIterator({}, dirPath);

	// invalid iterator or no entries found
	if (it == std::filesystem::directory_iterator())
	{
		resultPath[0] = '\0';
		return NULL_FILE_FIND_HANDLE;
	}

	FileFindHandle resultHandle = {};
	{
		LOCK_HOLDER(m_fileFindsSpinLock);
		resultHandle = (FileFindHandle)m_fileFindHandleManager.allocate();

		if (!resultHandle)
		{
			resultPath[0] = '\0';
			return NULL_FILE_FIND_HANDLE;
		}

		FileFind fileFind{};
		fileFind.m_iterator = it;

		const size_t idx = (size_t)resultHandle - 1;

		if (m_fileFinds.size() <= idx)
		{
			size_t newSize = idx;
			newSize += eastl::max<size_t>(1, newSize / 2);
			newSize = eastl::max<size_t>(16, newSize);
			m_fileFinds.resize(newSize);
		}

		m_fileFinds[idx] = fileFind;

		auto u8str = it->path().generic_u8string();
		strcpy_s(resultPath, u8str.length() + 1, u8str.c_str());

		return resultHandle;
	}
}

FileFindHandle RawFileSystem::findNext(FileFindHandle findHandle, char *resultPath) noexcept
{
	if (!findHandle)
	{
		resultPath[0] = '\0';
		return NULL_FILE_FIND_HANDLE;
	}

	std::filesystem::directory_iterator it;

	{
		LOCK_HOLDER(m_fileFindsSpinLock);

		it = m_fileFinds[findHandle - 1].m_iterator;

		// iterator was already invalid
		if (it == std::filesystem::directory_iterator())
		{
			resultPath[0] = '\0';
			return findHandle;
		}

		it = advanceDirIterator(it, nullptr);
		m_fileFinds[findHandle - 1].m_iterator = it;
	}

	// we either hit an error or reached the end. either way, we can no longer use this iterator
	if (it == std::filesystem::directory_iterator())
	{
		resultPath[0] = '\0';
	}
	else
	{
		auto u8str = it->path().generic_u8string();
		strcpy_s(resultPath, u8str.length() + 1, u8str.c_str());
	}

	return findHandle;
}

bool RawFileSystem::findIsDirectory(FileFindHandle findHandle) const noexcept
{
	if (!findHandle)
	{
		return NULL_FILE_FIND_HANDLE;
	}

	LOCK_HOLDER(m_fileFindsSpinLock);
	auto it = m_fileFinds[findHandle - 1].m_iterator;
	return it != std::filesystem::directory_iterator() && it->is_directory();
}

void RawFileSystem::findClose(FileFindHandle findHandle) noexcept
{
	if (!findHandle)
	{
		return;
	}

	LOCK_HOLDER(m_fileFindsSpinLock);

	m_fileFinds[findHandle - 1].m_iterator = {};
	m_fileFindHandleManager.free((uint32_t)findHandle);
}

std::filesystem::directory_iterator RawFileSystem::advanceDirIterator(std::filesystem::directory_iterator it, const char *initialPath) noexcept
{
	bool invalidIt = it == std::filesystem::directory_iterator();

	// input iterator is invalid but we have a path to initialize it with
	if (invalidIt && initialPath)
	{
		try
		{
			it = std::filesystem::directory_iterator(initialPath);
		}
		catch (std::filesystem::filesystem_error e)
		{
			return std::filesystem::directory_iterator();
		}

		// we couldnt fix the iterator :(
		if (it == std::filesystem::directory_iterator())
		{
			return std::filesystem::directory_iterator();
		}
		// we managed to create a valid iterator and the path length is also good!
		else if ((it->path().generic_u8string().length() + 1) <= k_maxPathLength)
		{
			return it;
		}
	}
	else if (invalidIt)
	{
		return std::filesystem::directory_iterator();
	}

	// increment and then skip past all entries with too long paths
	do
	{
		try
		{
			++it;
		}
		catch (...)
		{
			return std::filesystem::directory_iterator();
		}
	} while (it != std::filesystem::directory_iterator() && (it->path().generic_u8string().length() + 1) > k_maxPathLength);

	return it;
}
