
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "AssetBrowserWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/imgui_internal.h>
#include <unordered_map>
#include <graphics/Renderer.h>
#include <Engine.h>
#include <UUID.h>
#include <utility/Utility.h>
#include <file/FileDialog.h>
#include <Log.h>
#include <future>
#include <filesystem/VirtualFileSystem.h>


#ifdef WIN32
#include <ShObjIdl_core.h>
#endif

static std::future<void> s_future;

static void *thumbnailCallback(const char *path, void *userData)
{
	Engine *engine = (Engine *)userData;

#ifdef WIN32

	// maps from path to texture ID
	static std::unordered_map<std::string, ImTextureID> iconMap;
	// maps from icon hash to texture ID
	static std::unordered_map<size_t, ImTextureID> hasedIconMap;

	char resolvedPath[IFileSystem::k_maxPathLength];
	VirtualFileSystem::get().resolve(path, resolvedPath);

	auto it = iconMap.find(resolvedPath);
	if (it != iconMap.end())
	{
		return it->second;
	}

	ImTextureID resultTexID = {};

	// create IShellItemImageFactory from path
	IShellItemImageFactory *imageFactory = nullptr;
	std::wstring shellPath;
	for (size_t i = 0; resolvedPath[i]; ++i)
	{
		if (resolvedPath[i] == '/')
		{
			shellPath.push_back('\\');
		}
		else
		{
			shellPath.push_back(resolvedPath[i]);
		}
	}

	auto hr = SHCreateItemFromParsingName(shellPath.c_str(), nullptr, __uuidof(IShellItemImageFactory), (void **)&imageFactory);

	if (SUCCEEDED(hr))
	{
		SIZE size = { 128, 128 };

		//sz - Size of the image, SIIGBF_BIGGERSIZEOK - GetImage will stretch down the bitmap (preserving aspect ratio)
		HBITMAP hbmp;
		hr = imageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hbmp);
		if (SUCCEEDED(hr))
		{
			// query info about bitmap
			DIBSECTION ds;
			GetObject(hbmp, sizeof(ds), &ds);
			int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

			if (byteSize != 0)
			{
				// allocate memory and copy bitmap data to it
				uint8_t *data = (uint8_t *)malloc(byteSize);
				GetBitmapBits(hbmp, byteSize, data);

				size_t hash = 0;

				// convert from BGRA to RGBA
				for (size_t i = 0; i < byteSize; i += 4)
				{
					auto tmp = data[i];
					data[i] = data[i + 2];
					data[i + 2] = tmp;

					util::hashCombine(hash, *(uint32_t *)(data + i));
				}

				// found an identical image in our cache
				auto hashIt = hasedIconMap.find(hash);
				if (hashIt != hasedIconMap.end())
				{
					iconMap[resolvedPath] = hashIt->second;
					resultTexID = hashIt->second;
				}
				else
				{
					// create texture and store in maps
					auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, path, ds.dsBm.bmWidth, ds.dsBm.bmHeight);
					resultTexID = engine->getRenderer()->getImGuiTextureID(textureHandle);
					iconMap[resolvedPath] = resultTexID;
					hasedIconMap[hash] = resultTexID;
				}

				// dont forget to free our allocation!
				free(data);
			}


			DeleteObject(hbmp);
		}

		imageFactory->Release();
	}

	return resultTexID;

#else // WIN32
	static_assert(false);
#endif
}

AssetBrowserWindow::AssetBrowserWindow(Engine *engine) noexcept
	:m_engine(engine),
	m_fileBrowser(&VirtualFileSystem::get(), "/assets", thumbnailCallback, engine)
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
