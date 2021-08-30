#pragma once
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include "Handles.h"

class IFileSystem;

using ImGuiFileBrowserThumbnailCallback = void *(*)(const char *path, void *userData);

class ImGuiFileBrowser
{
public:
	explicit ImGuiFileBrowser(IFileSystem *fs, const char *currentPath, ImGuiFileBrowserThumbnailCallback thumbnailCallback, void *thumbnailCallbackUserData) noexcept;
	void draw() noexcept;
	eastl::string getCurrentPath() const noexcept;

private:
	IFileSystem *m_fs;
	ImGuiFileBrowserThumbnailCallback m_thumbnailCallback;
	void *m_thumbnailCallbackUserData;
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