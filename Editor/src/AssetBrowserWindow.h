#pragma once
#include <EASTL/vector.h>
#include <EASTL/atomic.h>
#include <string>
#include "importer/mesh/ModelImporter.h"
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
		ModelImporter::ImportOptions m_importOptions = {};
		std::string m_srcPath;
		std::string m_dstPath;
		bool m_done = false;
	};

	Engine *m_engine = nullptr;
	ImGuiFileBrowser m_fileBrowser;
	bool m_visible = true;
	ImportAssetTask m_importAssetTask;
	eastl::atomic_flag m_currentlyImporting = false;

	void importButton() noexcept;
};