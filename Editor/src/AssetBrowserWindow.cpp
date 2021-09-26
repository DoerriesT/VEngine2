#include "AssetBrowserWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/imgui_internal.h>
#include <graphics/Renderer.h>
#include <Engine.h>
#include <UUID.h>
#include <utility/Utility.h>
#include <file/FileDialog.h>
#include <Log.h>
#include <future>
#include <filesystem/VirtualFileSystem.h>
#include <asset/Asset.h>
#include <asset/AssetMetaDataRegistry.h>

static std::future<void> s_future;

static void *thumbnailCallback(const FileFindData &ffd, void *userData)
{
	Engine *engine = (Engine *)userData;
	auto *renderer = engine->getRenderer();
	uint32_t w = 128;
	uint32_t h = 128;
	return renderer->getImGuiTextureID(VirtualFileSystem::get().getIcon(ffd.m_path, engine->getRenderer(), &w, &h));
}

static bool fileFilterCallback(const FileFindData &ffd, void *userData)
{
	return VirtualFileSystem::get().exists((eastl::string(ffd.m_path) + ".meta").c_str());
}

static bool isDragDropSourceCallback(const FileFindData &ffd, void *userData)
{
	return ffd.m_isFile && AssetMetaDataRegistry::get()->isRegistered(AssetID(ffd.m_path + 8));
}

static void dragDropSourcePayloadCallback(const FileFindData &ffd, void *userData)
{
	assert(eastl::string_view(ffd.m_path).starts_with("/assets/"));

	AssetID assetID(ffd.m_path + 8);
	ImGui::SetDragDropPayload("ASSET_ID", &assetID, sizeof(assetID));
}

AssetBrowserWindow::AssetBrowserWindow(Engine *engine) noexcept
	:m_engine(engine),
	m_fileBrowser(
		&VirtualFileSystem::get(), 
		"/assets", 
		thumbnailCallback, 
		fileFilterCallback, 
		isDragDropSourceCallback,
		dragDropSourcePayloadCallback,
		engine)
{
}

void AssetBrowserWindow::draw() noexcept
{
	if (!m_visible)
	{
		return;
	}

	auto &vfs = VirtualFileSystem::get();

	ImGui::Begin("Asset Browser");
	{
		importButton();

		m_fileBrowser.draw();
	}
	ImGui::End();
}

void AssetBrowserWindow::setVisible(bool visible) noexcept
{
	m_visible = visible;
}

bool AssetBrowserWindow::isVisible() const noexcept
{
	return m_visible;
}

void AssetBrowserWindow::importButton() noexcept
{
	if (ImGui::Button("Import"))
	{
		FileDialog::FileExtension extensions[]
		{
			{ "Wavefront OBJ", "*.obj" },
			{ "glTF", "*.gltf" }
		};

		FileDialog::FileDialogParams dialogParams{};
		dialogParams.m_fileExtensionCount = std::size(extensions);
		dialogParams.m_fileExtensions = extensions;
		dialogParams.m_multiSelection = false;
		dialogParams.m_fileSelection = true;
		dialogParams.m_save = false;

		eastl::vector<std::filesystem::path> selectedFiles;
		uint32_t fileExtensionIndex;
		if (FileDialog::showFileDialog(dialogParams, selectedFiles, fileExtensionIndex))
		{
			eastl::string srcPath = std::filesystem::relative(selectedFiles[0], std::filesystem::current_path()).generic_u8string().c_str();
			Log::info("Importing File: \"%s\"", srcPath.c_str());

			m_importAssetTask = {};
			m_importAssetTask.m_srcPath = srcPath;
			m_importAssetTask.m_dstPath = m_fileBrowser.getCurrentPath() + "/" + selectedFiles[0].filename().replace_extension().generic_u8string().c_str();

			switch (fileExtensionIndex)
			{
			case 0:
				m_importAssetTask.m_importOptions.m_fileType = AssetImporter::FileType::WAVEFRONT_OBJ; break;
			case 1:
				m_importAssetTask.m_importOptions.m_fileType = AssetImporter::FileType::GLTF; break;
			default:
				assert(false);
				break;
			}

			ImGui::OpenPopup("Import Asset");
		}
	}

	bool startedImport = false;

	if (ImGui::BeginPopupModal("Import Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Checkbox("Merge by Material", &m_importAssetTask.m_importOptions.m_mergeByMaterial);
		ImGui::Checkbox("Invert UV Y-Component", &m_importAssetTask.m_importOptions.m_invertTexCoordY);
		ImGui::Checkbox("Import Meshes", &m_importAssetTask.m_importOptions.m_importMeshes);
		ImGui::Checkbox("Import Skeletons", &m_importAssetTask.m_importOptions.m_importSkeletons);
		ImGui::Checkbox("Import Animations", &m_importAssetTask.m_importOptions.m_importAnimations);

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			startedImport = true;
			m_currentlyImporting.test_and_set();
			s_future = std::async(std::launch::async, [this]()
				{
					AssetImporter::importAsset(m_importAssetTask.m_importOptions, m_engine->getPhysics(), m_importAssetTask.m_srcPath.c_str(), m_importAssetTask.m_dstPath.c_str());
					m_currentlyImporting.clear();
				});

			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (startedImport)
	{
		ImGui::OpenPopup("importing_asset_progess_indicator");
	}

	if (ImGui::BeginPopupModal("importing_asset_progess_indicator", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
	{
		//ImGui::LoadingIndicatorCircle("Importing Asset", 30.0f, ImGui::GetStyleColorVec4(ImGuiCol_Header), ImGui::GetStyleColorVec4(ImGuiCol_WindowBg), 8, 1.0f);

		if (!m_currentlyImporting.test())
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}
