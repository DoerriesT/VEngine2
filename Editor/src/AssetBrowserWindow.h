#pragma once
#include <EASTL/vector.h>
#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include "importer/AssetImporter.h"
#include <graphics/imgui/imgui_file_browser.h>

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
		AssetImporter::ImportOptions m_importOptions = {};
		eastl::string m_srcPath;
		eastl::string m_dstPath;
		bool m_done = false;
	};

	Engine *m_engine = nullptr;
	ImGuiFileBrowser m_fileBrowser;
	bool m_visible = true;
	ImportAssetTask m_importAssetTask;
	eastl::atomic_flag m_currentlyImporting = false;
	size_t m_addAssetPopupSelectedAssetType = 0;
	char m_addAssetPopupAssetName[256];

	void importButton() noexcept;
};