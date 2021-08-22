#include "VirtualFileSystem.h"
#include "RawFileSystem.h"
#include "Log.h"

VirtualFileSystem &VirtualFileSystem::get() noexcept
{
	static VirtualFileSystem vfs;
	return vfs;
}

bool VirtualFileSystem::mount(const char *nativePath, const char *mountName) noexcept
{
	m_mountPoints[mountName].push_back(nativePath);
	return true;
}

void VirtualFileSystem::unmount(const char *mountName) noexcept
{
	auto it = m_mountPoints.find(mountName);
	if (it != m_mountPoints.end())
	{
		auto &v = it->second;
		v.erase(eastl::remove_if(v.begin(), v.end(), [&](const auto &str) { return str == mountName; }), v.end());
	}
}

void VirtualFileSystem::unmountAll() noexcept
{
	m_mountPoints.clear();
}

bool VirtualFileSystem::resolve(const char *path, char *result) const noexcept
{
	// we only support absolute paths
	if (!path || path[0] != '/')
	{
		return false;
	}

	const size_t inputPathLen = strlen(path);

	if ((inputPathLen + 1) > k_maxPathLength)
	{
		return false;
	}

	// get mount name
	char mountName[k_maxPathLength];
	size_t mountNameLength = 0;
	{
		// skip past the first /
		const char *tmp = path + 1;

		while (*tmp && *tmp != '/')
		{
			mountName[mountNameLength++] = *tmp;
			++tmp;
		}
		// write null terminator
		mountName[mountNameLength] = '\0';
	}

	// get path remainder (will start with '/')
	const size_t remainderOffset = mountNameLength + 1; /*skip past the beginning '/' */
	const char *remainder = path + remainderOffset;
	size_t remainderLen = inputPathLen - remainderOffset;

	// special case for the "root" mount directory
	if (remainderOffset == mountNameLength)
	{
		remainder = "";
		remainderLen = 0;
	}

	auto it = m_mountPoints.find(mountName);
	if (it == m_mountPoints.end())
	{
		return false;
	}

	auto &rfs = RawFileSystem::get();

	const auto &mountPaths = it->second;

	for (auto mountPathIt = mountPaths.rbegin(); mountPathIt != mountPaths.rend(); ++mountPathIt)
	{
		const auto &mountPath = *mountPathIt;
		const size_t mountPathLen = mountPath.length();

		if (mountPathLen + remainderLen + 1 > k_maxPathLength)
		{
			Log::err("VirtualFileSystem: The sum of the length of the mount point path \"%s\" and the virtual path \"%s\" exceed the maximum path length (%u)", mountPath.c_str(), remainder, (unsigned)k_maxPathLength);
		}

		size_t curLength = 0;
		memcpy(result, mountPath.c_str(), mountPathLen);
		memcpy(result + mountPathLen, remainder, remainderLen);
		result[mountPathLen + remainderLen] = '\0';

		if (rfs.exists(result))
		{
			return true;
		}
	}

	return false;
}

bool VirtualFileSystem::unresolve(const char *path, char *result) const noexcept
{
	// empty paths result in empty paths
	if (!path || !path[0])
	{
		result[0] = '\0';
		return true;
	}

	for (const auto &p : m_mountPoints)
	{
		const auto &mountPaths = p.second;

		for (auto mountPathIt = mountPaths.rbegin(); mountPathIt != mountPaths.rend(); ++mountPathIt)
		{
			const auto &mountPath = *mountPathIt;

			if (memcmp(mountPath.c_str(), path, mountPath.length()) == 0)
			{
				// start with the forward slash
				result[0] = '/';
				// copy mount name into result
				size_t mountNameLen = strlen(p.first);
				memcpy(result + 1, p.first, mountNameLen + 1);
				// append everything after the mount path to the result
				memcpy(result + 1 + mountNameLen, path + mountPath.length(), strlen(path) - mountPath.length() + 1);
				return true;
			}
		}
	}

	return false;
}

bool VirtualFileSystem::exists(const char *path) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(path, resolvedPath);
	return RawFileSystem::get().exists(resolvedPath);
}

bool VirtualFileSystem::isDirectory(const char *path) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(path, resolvedPath);
	return RawFileSystem::get().isDirectory(resolvedPath);
}

bool VirtualFileSystem::isFile(const char *path) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(path, resolvedPath);
	return RawFileSystem::get().isFile(resolvedPath);
}

bool VirtualFileSystem::createDirectoryHierarchy(const char *path) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(path, resolvedPath);
	return RawFileSystem::get().createDirectoryHierarchy(resolvedPath);
}

bool VirtualFileSystem::rename(const char *path, const char *newName) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(path, resolvedPath);
	return RawFileSystem::get().rename(resolvedPath, newName);
}

bool VirtualFileSystem::remove(const char *path) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(path, resolvedPath);
	return RawFileSystem::get().remove(resolvedPath);
}

FileHandle VirtualFileSystem::open(const char *filePath, FileMode mode, bool binary) noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(filePath, resolvedPath);
	return RawFileSystem::get().open(resolvedPath, mode, binary);
}

uint64_t VirtualFileSystem::size(FileHandle fileHandle) const noexcept
{
	return RawFileSystem::get().size(fileHandle);
}

uint64_t VirtualFileSystem::size(const char *filePath) const noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(filePath, resolvedPath);
	return RawFileSystem::get().size(resolvedPath);
}

uint64_t VirtualFileSystem::read(FileHandle fileHandle, size_t bufferSize, void *buffer) const noexcept
{
	return RawFileSystem::get().read(fileHandle, bufferSize, buffer);
}

uint64_t VirtualFileSystem::write(FileHandle fileHandle, size_t bufferSize, const void *buffer) const noexcept
{
	return RawFileSystem::get().write(fileHandle, bufferSize, buffer);
}

void VirtualFileSystem::close(FileHandle fileHandle) noexcept
{
	RawFileSystem::get().close(fileHandle);
}

bool VirtualFileSystem::readFile(const char *filePath, size_t bufferSize, void *buffer) noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(filePath, resolvedPath);
	return RawFileSystem::get().readFile(resolvedPath, bufferSize, buffer);
}

bool VirtualFileSystem::writeFile(const char *filePath, size_t bufferSize, const void *buffer) noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(filePath, resolvedPath);
	return RawFileSystem::get().writeFile(resolvedPath, bufferSize, buffer);
}

FileFindHandle VirtualFileSystem::findFirst(const char *dirPath, FileFindData *result) noexcept
{
	char resolvedPath[k_maxPathLength] = {};
	resolve(dirPath, resolvedPath);

	FileFindData tmpFindData{};
	auto handle = RawFileSystem::get().findFirst(resolvedPath, &tmpFindData);
	
	*result = {};

	unresolve(tmpFindData.m_path, result->m_path);
	result->m_isDirectory = tmpFindData.m_isDirectory;
	result->m_isFile = tmpFindData.m_isFile;

	return handle;
}

bool VirtualFileSystem::findNext(FileFindHandle findHandle, FileFindData *result) noexcept
{
	FileFindData tmpFindData{};
	bool res = RawFileSystem::get().findNext(findHandle, &tmpFindData);
	
	*result = {};

	unresolve(tmpFindData.m_path, result->m_path);
	result->m_isDirectory = tmpFindData.m_isDirectory;
	result->m_isFile = tmpFindData.m_isFile;

	return res;
}

void VirtualFileSystem::findClose(FileFindHandle findHandle) noexcept
{
	RawFileSystem::get().findClose(findHandle);
}
