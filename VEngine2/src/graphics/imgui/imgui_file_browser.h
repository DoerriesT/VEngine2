#pragma once
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include "Handles.h"

class IFileSystem;

class ImGuiFileBrowser
{
public:
	using ThumbnailCallback = void *(*)(const char *path, void *userData);
	using FileFilterCallback = bool (*)(const char *path, void *userData);

	explicit ImGuiFileBrowser(IFileSystem *fs, const char *currentPath, ThumbnailCallback thumbnailCallback, FileFilterCallback filterCallback, void *callbackUserData) noexcept;
	void draw() noexcept;
	eastl::string getCurrentPath() const noexcept;

private:
	IFileSystem *m_fs;
	ThumbnailCallback m_thumbnailCallback;
	FileFilterCallback m_fileFilterCallback;
	void *m_callbackUserData;
	eastl::string m_currentPath;
	eastl::string m_subDirPopupPath;
	eastl::vector<eastl::string> m_currentPathSegments;
	eastl::vector<eastl::string> m_history;
	int m_historyPointer = 0;
	char m_searchText[256] = {};
	char m_searchTextLower[256] = {};

	void drawPathMenu(eastl::string *newPath, bool *newPathIsFromHistory) noexcept;
	void drawMainArea(eastl::string *newPath) noexcept;
	void drawTreeNode(const char *path, eastl::string *newPath) noexcept;
	void updatePathSegments() noexcept;
};