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
#include <asset/ScriptAsset.h>
#include <asset/AssetManager.h>

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
	const bool importButtonClicked = ImGui::Button("Import");
	ImGui::SameLine();
	const bool addButtonClicked = ImGui::Button("Add");

	// import button
	{
		if (importButtonClicked)
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
			ImGui::Checkbox("Cook Convex Physics Mesh", &m_importAssetTask.m_importOptions.m_cookConvexPhysicsMesh);
			ImGui::Checkbox("Cook Triangle Physics Mesh", &m_importAssetTask.m_importOptions.m_cookTrianglePhysicsMesh);

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

	// add button
	{
		if (addButtonClicked)
		{
			ImGui::OpenPopup("Add Asset");
			m_addAssetPopupAssetName[0] = '\0';
		}

		if (ImGui::BeginPopupModal("Add Asset"))
		{
			const char *assetTypeNames[]
			{
				"Script"
			};

			AssetType assetTypes[]
			{
				ScriptAssetData::k_assetType
			};

			assert(m_addAssetPopupSelectedAssetType < eastl::size(assetTypeNames));
			if (ImGui::BeginCombo("Type", assetTypeNames[m_addAssetPopupSelectedAssetType]))
			{
				size_t selectedAssetType = m_addAssetPopupSelectedAssetType;

				for (size_t i = 0; i < eastl::size(assetTypeNames); ++i)
				{
					bool selected = i == m_addAssetPopupSelectedAssetType;
					if (ImGui::Selectable(assetTypeNames[i], selected))
					{
						selectedAssetType = i;
					}
				}
				ImGui::EndCombo();

				m_addAssetPopupSelectedAssetType = selectedAssetType;
			}

			ImGui::InputText("Name", m_addAssetPopupAssetName, sizeof(m_addAssetPopupAssetName));

			ImGui::Separator();

			auto proposedAssetPath = m_fileBrowser.getCurrentPath();
			proposedAssetPath += "/";
			proposedAssetPath += m_addAssetPopupAssetName;

			bool canCreateAsset = m_addAssetPopupAssetName[0] != '\0' && !VirtualFileSystem::get().exists(proposedAssetPath.c_str());
			for (size_t i = 0; m_addAssetPopupAssetName[i] != '\0'; ++i)
			{
				switch (m_addAssetPopupAssetName[i])
				{
				case '\\':
				case '/':
				case ':':
				case '*':
				case '?':
				case '"':
				case '<':
				case '>':
				case '|':
					canCreateAsset = false;
					break;
				default:
					break;
				}
			}

			if (!canCreateAsset)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			bool buttonOKPressed = ImGui::Button("OK", ImVec2(120, 0));
			if (!canCreateAsset)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}


			if (buttonOKPressed && canCreateAsset)
			{
				VirtualFileSystem::get().writeFile(proposedAssetPath.c_str(), 0, nullptr, true);
				AssetManager::get()->createAsset(assetTypes[m_addAssetPopupSelectedAssetType], proposedAssetPath.c_str(), proposedAssetPath.c_str());
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
	}
}
