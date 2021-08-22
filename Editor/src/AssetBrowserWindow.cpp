#include "AssetBrowserWindow.h"
#include <graphics/imgui/imgui.h>
#include <graphics/imgui/imgui_internal.h>
#include <ShObjIdl_core.h>
#include <windows.h>
#include <thumbcache.h>
#include <unordered_map>
#include <graphics/Renderer.h>
#include <Engine.h>
#include <UUID.h>
#include <utility/Utility.h>
#include <file/FileDialog.h>
#include <Log.h>
#include <future>
#include <filesystem/Path.h>
#include <filesystem/VirtualFileSystem.h>

static std::future<void> s_future;

static TextureHandle getThumbNail(const std::string &path, Engine *engine)
{
	static std::unordered_map<std::string, TextureHandle> iconMap;

	auto it = iconMap.find(path);
	if (it != iconMap.end())
	{
		return it->second;
	}

#if 0
	static std::unordered_map<int, TextureHandle> iconIndexToTexMap;

	std::error_code ec;
	iconMap[pathU8] = {};

	DWORD attrs = 0;
	UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
	if (!std::filesystem::exists(path, ec)) {
		flags |= SHGFI_USEFILEATTRIBUTES;
		attrs = FILE_ATTRIBUTE_DIRECTORY;
	}

	SHFILEINFOW fileInfo = { 0 };
	std::wstring pathW = path.wstring();
	for (int i = 0; i < pathW.size(); i++)
		if (pathW[i] == '/')
			pathW[i] = '\\';
	SHGetFileInfoW(pathW.c_str(), attrs, &fileInfo, sizeof(SHFILEINFOW), flags);

	if (fileInfo.hIcon == nullptr)
		return {};

	// check if icon is already loaded
	auto idxIt = iconIndexToTexMap.find(fileInfo.iIcon);
	if (idxIt != iconIndexToTexMap.end())
	{
		iconMap[pathU8] = idxIt->second;
		return idxIt->second;
	}

	ICONINFO iconInfo = { 0 };
	GetIconInfo(fileInfo.hIcon, &iconInfo);

	if (iconInfo.hbmColor == nullptr)
		return {};

	DIBSECTION ds;
	GetObject(iconInfo.hbmColor, sizeof(ds), &ds);
	int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

	if (byteSize == 0)
		return {};

	uint8_t *data = (uint8_t *)malloc(byteSize);
	GetBitmapBits(iconInfo.hbmColor, byteSize, data);

	auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, path.generic_string().c_str(), ds.dsBm.bmWidth, ds.dsBm.bmHeight);
	iconMap[pathU8] = textureHandle;
	iconIndexToTexMap[fileInfo.iIcon] = textureHandle;

	free(data);

	return textureHandle;

	#elseif 0
		static std::unordered_map<TUUID, TextureHandle, UUIDHash> iconIDToTexMap;

	TextureHandle resultTex = {};

	// create IShellItem from path
	IShellItem *shellItem = nullptr;
	auto hr = SHCreateItemFromParsingName(path.native().c_str(), nullptr, __uuidof(IShellItem), (void **)&shellItem);
	if (SUCCEEDED(hr))
	{
		// get thumbnail cache
		IThumbnailCache *pThumbCache;
		hr = CoCreateInstance(CLSID_LocalThumbnailCache, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IThumbnailCache), (void **)&pThumbCache);
		if (SUCCEEDED(hr))
		{
			// get thumbnail
			WTS_THUMBNAILID thumbID{};
			ISharedBitmap *pBitmap;
			hr = pThumbCache->GetThumbnail(shellItem, 128, WTS_EXTRACT, &pBitmap, nullptr, &thumbID);
			if (SUCCEEDED(hr))
			{
				// check if we already loaded this thumbnail
				TUUID thumbUUID = TUUID(thumbID.rgbKey);
				auto idIt = iconIDToTexMap.find(thumbUUID);
				if (idIt != iconIDToTexMap.end())
				{
					iconMap[pathU8] = idIt->second;
					resultTex = idIt->second;
				}
				// we haven't loaded this thumbnail yet
				else
				{
					// get bitmap from thumbnail
					HBITMAP hBitmap;
					hr = pBitmap->GetSharedBitmap(&hBitmap);
					if (SUCCEEDED(hr))
					{
						// query info about bitmap
						DIBSECTION ds;
						GetObject(hBitmap, sizeof(ds), &ds);
						int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

						if (byteSize != 0)
						{
							// allocate memory and copy bitmap data to it
							uint8_t *data = (uint8_t *)malloc(byteSize);
							GetBitmapBits(hBitmap, byteSize, data);

							// convert from BGRA to RGBA
							for (size_t i = 0; i < byteSize; i += 4)
							{
								auto tmp = data[i];
								data[i] = data[i + 2];
								data[i + 2] = tmp;
							}

							// create texture and store in maps
							auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, pathU8.c_str(), ds.dsBm.bmWidth, ds.dsBm.bmHeight);
							iconMap[pathU8] = textureHandle;
							iconIDToTexMap[thumbUUID] = textureHandle;

							// dont forget to free our allocation!
							free(data);

							resultTex = textureHandle;
						}
					}
				}
			}
			else
			{
				printf("");
			}

			if (pBitmap)
			{
				pBitmap->Release();
			}
		}

		if (pThumbCache)
		{
			pThumbCache->Release();
		}
	}

	return resultTex;
#else
	TextureHandle resultTex = {};

	static std::unordered_map<size_t, TextureHandle> hasedIconMap;

	// create IShellItemImageFactory  from path
	IShellItemImageFactory *imageFactory = nullptr;
	std::wstring newStr;
	for (auto c : path)
	{
		if (c == '/')
		{
			newStr.push_back('\\');
		}
		else
		{
			newStr.push_back(c);
		}
	}

	auto hr = SHCreateItemFromParsingName(newStr.c_str(), nullptr, __uuidof(IShellItemImageFactory), (void **)&imageFactory);

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
					iconMap[path] = hashIt->second;
					resultTex = hashIt->second;
				}
				else
				{
					// create texture and store in maps
					auto textureHandle = engine->getRenderer()->loadRawRGBA8(byteSize, (char *)data, path.c_str(), ds.dsBm.bmWidth, ds.dsBm.bmHeight);
					iconMap[path] = textureHandle;
					hasedIconMap[hash] = textureHandle;

					resultTex = textureHandle;
				}

				// dont forget to free our allocation!
				free(data);
			}


			DeleteObject(hbmp);
		}

		imageFactory->Release();
	}

	return resultTex;
#endif
}

AssetBrowserWindow::AssetBrowserWindow(Engine *engine) noexcept
	:m_engine(engine)
{
	updatePathSegments();
	m_history.push_back(m_currentPath);
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

		auto newPath = m_currentPath;
		bool newPathIsFromHistory = false;

		// history forward/backward buttons
		{
			bool disabledBackButton = !(m_historyPointer > 0);
			bool disabledForwardButton = !((m_historyPointer + 1) < m_history.size());

			if (disabledBackButton)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::ArrowButton("<##path_history_backward", ImGuiDir_Left))
			{
				if (!disabledBackButton)
				{
					--m_historyPointer;
					newPath = m_history[m_historyPointer];
					newPathIsFromHistory = true;
				}
			}
			if (disabledBackButton)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}

			ImGui::SameLine();

			if (disabledForwardButton)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::ArrowButton(">##path_history_forward", ImGuiDir_Right))
			{
				if (!disabledForwardButton)
				{
					++m_historyPointer;
					newPath = m_history[m_historyPointer];
					newPathIsFromHistory = true;
				}
			}
			if (disabledForwardButton)
			{
				ImGui::PopItemFlag();
				ImGui::PopStyleVar();
			}
		}

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		// current path menu
		{
			bool showSubDirPopup = false;

			for (const auto &p : m_currentPathSegments)
			{
				ImGui::PushID(&p);
				if (ImGui::Button(Path::getFileName(p.c_str())))
				{
					newPath = p;
				}
				ImGui::SameLine();
				ImGui::PopID();

				bool hasSubDirs = false;

				char tmp[IFileSystem::k_maxPathLength];
				auto findHandle = vfs.findFirst(p.c_str(), tmp);

				while (tmp[0])
				{
					if (vfs.findIsDirectory(findHandle))
					{
						hasSubDirs = true;
						break;
					}
					findHandle = vfs.findNext(findHandle, tmp);
				}
				vfs.findClose(findHandle);

				if (hasSubDirs)
				{
					ImGui::PushID(p.c_str());
					bool buttonClicked = ImGui::ArrowButton("##>", ImGuiDir_Right);
					ImGui::PopID();
					if (buttonClicked)
					{
						showSubDirPopup = true;
						m_subDirPopupPath = p;
					}
					ImGui::SameLine();
				}
			}

			if (showSubDirPopup)
			{
				ImGui::OpenPopup("jump_to_sub_dir_popup");
			}

			if (ImGui::BeginPopup("jump_to_sub_dir_popup"))
			{
				char tmp[IFileSystem::k_maxPathLength];
				auto findHandle = vfs.findFirst(m_subDirPopupPath.c_str(), tmp);
				while (tmp[0])
				{
					if (vfs.findIsDirectory(findHandle))
					{
						if (ImGui::Selectable(tmp))
						{
							newPath = tmp;
						}
					}
					findHandle = vfs.findNext(findHandle, tmp);
				}
				vfs.findClose(findHandle);

				ImGui::EndPopup();
			}

			ImGui::NewLine();
		}

		if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("##dir_tree", ImGuiTableColumnFlags_WidthFixed, 125.0f);
			ImGui::TableSetupColumn("##content", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			// directory tree
			{
				ImGui::TableSetColumnIndex(0);
				ImGui::BeginChild("##dir_tree_container");
				{
					renderTreeNode(m_currentPathSegments[0], &newPath);
				}
				ImGui::EndChild();
			}

			// content
			{
				ImGui::TableSetColumnIndex(1);
				ImGui::BeginChild("##contentContainer");
				{
					// search field
					{
						ImGui::Text("Search");
						ImGui::SameLine();
						ImGui::InputText("##Search", m_searchText, sizeof(m_searchText));
						for (size_t i = 0; ; ++i)
						{
							if (m_searchText[i] == '\0')
							{
								m_searchTextLower[i] = '\0';
								break;
							}
							m_searchTextLower[i] = std::tolower(m_searchText[i]);
						}
					}

					ImGui::Separator();
					ImGui::NewLine();
					
					std::string_view searchTextView = std::string_view(m_searchTextLower);
					
					float itemSize = 128.0f;
					auto contentRegionAvail = ImGui::GetContentRegionAvail();
					int columns = (int)(contentRegionAvail.x / 128.0f);
					columns = columns < 1 ? 1 : columns;
					ImGui::Columns(columns, nullptr, false);
					
					for (int isFileOnlyRun = 0; isFileOnlyRun < 2; ++isFileOnlyRun)
					{
						char tmp[IFileSystem::k_maxPathLength];
						auto findHandle = vfs.findFirst(m_currentPath.c_str(), tmp);

						while (tmp[0])
						{
							if (isFileOnlyRun == 0 && !vfs.isDirectory(tmp))
							{
								findHandle = vfs.findNext(findHandle, tmp);
								continue;
							}
							if (isFileOnlyRun == 1 && !vfs.isFile(tmp))
							{
								findHandle = vfs.findNext(findHandle, tmp);
								continue;
							}

							std::string displayName = Path::getFileName(tmp);
							std::string displayNameLower = displayName;
							std::transform(displayNameLower.begin(), displayNameLower.end(), displayNameLower.begin(), [](unsigned char c) { return std::tolower(c); });

							if (!searchTextView.empty() && displayNameLower.find(searchTextView) == std::string::npos)
							{
								findHandle = vfs.findNext(findHandle, tmp);
								continue;
							}

							ImGui::PushID(displayName.c_str());

							ImGui::BeginGroup();
							{
								char resolvedPath[IFileSystem::k_maxPathLength];
								vfs.resolve(tmp, resolvedPath);
								auto thumb = getThumbNail(resolvedPath, m_engine);
								if (!thumb)
								{
									ImGui::Button("???", ImVec2(itemSize - 20.0f, itemSize - 16));
								}
								else
								{
									ImGui::Image(m_engine->getRenderer()->getImGuiTextureID(thumb), ImVec2(itemSize - 20.0f, itemSize - 16));
								}

								ImGui::TextWrapped(displayName.c_str());
							}
							ImGui::EndGroup();
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip(displayName.c_str());
							}
							if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
							{
								if (vfs.isDirectory(tmp))
								{
									newPath = tmp;
								}
							}

							ImGui::PopID();
							ImGui::NextColumn();

							findHandle = vfs.findNext(findHandle, tmp);
						}
						vfs.findClose(findHandle);
					}
				}
				ImGui::EndChild();
			}

			ImGui::EndTable();
		}

		// update path
		if (m_currentPath != newPath)
		{
			if (!newPathIsFromHistory)
			{
				if ((m_historyPointer + 1) != m_history.size())
				{
					m_history.resize(m_historyPointer + 1);
				}
		
				m_history.push_back(newPath);
				++m_historyPointer;
			}
			m_currentPath = newPath;
			updatePathSegments();
		}

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

void AssetBrowserWindow::updatePathSegments() noexcept
{
	m_currentPathSegments.clear();
	const size_t pathSegmentCount = Path::getSegmentCount(m_currentPath.c_str());
	m_currentPathSegments.reserve(pathSegmentCount);

	for (size_t i = 0; i < pathSegmentCount; ++i)
	{
		size_t offset = Path::getFirstNSegments(m_currentPath.c_str(), i + 1);
		m_currentPathSegments.push_back(m_currentPath.substr(0, offset));
	}
}

void AssetBrowserWindow::renderTreeNode(const std::string &path, std::string *newPath) noexcept
{
	auto &vfs = VirtualFileSystem::get();

	if (!vfs.isDirectory(path.c_str()))
	{
		return;
	}

	bool isSelected = m_currentPath == path;
	bool isParentOfCurrentPath = m_currentPath.find(path) != std::string::npos; // TODO: bad hack, find something better
	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoAutoOpenOnLog;
	treeNodeFlags |= isSelected ? ImGuiTreeNodeFlags_Selected : 0;
	treeNodeFlags |= isParentOfCurrentPath ? ImGuiTreeNodeFlags_DefaultOpen : 0;

	bool nodeOpen = ImGui::TreeNodeEx(Path::getFileName(path.c_str()), treeNodeFlags);
	if (ImGui::IsItemClicked())
	{
		*newPath = path;
	}

	if (nodeOpen)
	{
		char entry[IFileSystem::k_maxPathLength];
		auto findHandle = vfs.findFirst(path.c_str(), entry);
		while (entry[0])
		{
			if (vfs.findIsDirectory(findHandle))
			{
				renderTreeNode(entry, newPath);
			}
			findHandle = vfs.findNext(findHandle, entry);
		}
		vfs.findClose(findHandle);

		ImGui::TreePop();
	}
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
			Log::info(("Importing File: " + selectedFiles[0].u8string()).c_str());

			m_importAssetTask = {};
			m_importAssetTask.m_srcPath = selectedFiles[0].u8string();
			m_importAssetTask.m_dstPath = "assets/meshes/" + selectedFiles[0].filename().replace_extension().u8string();

			switch (fileExtensionIndex)
			{
			case 0:
				m_importAssetTask.m_importOptions.m_fileType = ModelImporter::FileType::WAVEFRONT_OBJ; break;
			case 1:
				m_importAssetTask.m_importOptions.m_fileType = ModelImporter::FileType::GLTF; break;
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
					ModelImporter::importModel(m_importAssetTask.m_importOptions, m_engine->getPhysics(), m_importAssetTask.m_srcPath.c_str(), m_importAssetTask.m_dstPath.c_str());
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
