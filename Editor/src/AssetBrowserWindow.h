#pragma once
#include <EASTL/vector.h>
#include <EASTL/atomic.h>
#include <string>
#include "importer/mesh/ModelImporter.h"

class Engine;

class AssetBrowserWindow
{
public:
	explicit AssetBrowserWindow(Engine *engine) noexcept;
	void draw() noexcept;
	void setVisible(bool visible) noexcept;
	bool isVisible() const noexcept;

private:
	struct ImportAssetTask
	{
		ModelImporter::ImportOptions m_importOptions = {};
		std::string m_srcPath;
		std::string m_dstPath;
		bool m_done = false;
	};

	Engine *m_engine = nullptr;
	std::string m_currentPath = "/assets";
	std::string m_subDirPopupPath;
	char m_searchText[256] = {};
	char m_searchTextLower[256] = {};
	eastl::vector<std::string> m_currentPathSegments;
	eastl::vector<std::string> m_history;
	int m_historyPointer = 0;
	bool m_visible = true;
	ImportAssetTask m_importAssetTask;
	eastl::atomic_flag m_currentlyImporting = false;

	void updatePathSegments() noexcept;
	void renderTreeNode(const std::string &path, std::string *newPath) noexcept;
	void importButton() noexcept;
};