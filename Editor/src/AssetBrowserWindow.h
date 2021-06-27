#pragma once
#include <filesystem>
#include <EASTL/vector.h>

class Engine;

class AssetBrowserWindow
{
public:
	explicit AssetBrowserWindow(Engine *engine) noexcept;
	void draw() noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	Engine *m_engine = nullptr;
	std::filesystem::path m_currentPath = std::filesystem::current_path();
	std::filesystem::path m_subDirPopupPath;
	char m_searchText[256] = {};
	char m_searchTextLower[256] = {};
	eastl::vector<std::filesystem::path> m_currentPathSegments;
	eastl::vector<std::filesystem::path> m_history;
	int m_historyPointer = 0;
	bool m_visible = true;

	void updatePathSegments() noexcept;
	void renderTreeNode(const std::filesystem::path &path, std::filesystem::path *newPath) noexcept;
};